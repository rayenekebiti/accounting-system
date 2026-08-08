#!/usr/bin/env bash
# ptest.sh — persistence + integrity + crash-recovery gate (deterministic, isolated).
#
# Two layers:
#   1. In-process suite (ACCT_PTEST=suite): round-trip, journal replay/corrupt/
#      stale/torn, partial-write recovery, totals==Σlines, Money determinism,
#      duplicate-number detection, open/append/close stability.
#   2. REAL cross-process crash recovery: a child appends a sentinel and is hard-
#      killed (std::_Exit) at each write step via ACCT_CRASH_POINT; a fresh process
#      then reopens the file and must find the sentinel intact. This is the only
#      honest test of power-loss / app-kill durability — no in-process mock can
#      prove the OS-level write ordering holds across an actual process death.
#
# Usage:  bash tools/ptest.sh
# Exit 0 = everything passed.

set -u
cd "$(dirname "$0")/.." || exit 2
BUILD=build
EXE="$BUILD/AccountingQuick.exe"
win() { cygpath -m "$1"; }
# Filter the harmless Qt translation-catalog warning the deployed build prints.
QUIET='catalogs.json|Translations will not be available'

echo "== building =="
cmake --build "$BUILD" --target AccountingQuick 2>&1 | grep -iE "error:|error C" && { echo "BUILD FAILED"; exit 1; }
[ -f "$EXE" ] || { echo "missing $EXE"; exit 1; }

fail=0

# ── Layer 1: in-process deterministic suite ──────────────────────────────────
echo
echo "== in-process suite =="
SUITE_DIR="$BUILD/.ptest_suite"
rm -rf "$SUITE_DIR"; mkdir -p "$SUITE_DIR"
env ACCT_PTEST=suite ACCT_DATA_DIR="$(win "$PWD/$SUITE_DIR")" "./$EXE" 2>&1 | grep -vE "$QUIET"
suite_status=${PIPESTATUS[0]}
[ "$suite_status" -eq 0 ] || { echo "  -> suite FAILED ($suite_status)"; fail=1; }

# ── Layer 2: real cross-process crash recovery ───────────────────────────────
# For each crash point the child must die with code 99 (our std::_Exit value);
# the follow-up verify process must then recover the sentinel (exit 0).
echo
echo "== cross-process crash recovery =="
crash_case() {
  point="$1"
  dir="$BUILD/.ptest_crash_$point"
  rm -rf "$dir"; mkdir -p "$dir"
  d="$(win "$PWD/$dir")"

  # 1. Crash the writer at $point.
  env ACCT_PTEST=crashwrite ACCT_CRASH_POINT="$point" ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
  wrote=$?

  # 2. Reopen in a brand-new process and verify the sentinel survived.
  vout=$(env ACCT_PTEST=crashverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
  vstatus=$?

  if [ "$wrote" -eq 99 ] && [ "$vstatus" -eq 0 ]; then
    echo "  [PASS] crash@$point: writer killed (99), sentinel recovered"
  else
    echo "  [FAIL] crash@$point: writer=$wrote verify=$vstatus  ($vout)"
    fail=1
  fi
}

crash_case afterJournal      # journal durable, main file untouched   → replay restores
crash_case afterMainWrite    # record written, header not committed    → replay re-applies
crash_case afterHeader       # committed, journal not yet deleted      → data intact, journal dropped

# ── Layer 2b: real cross-process crash recovery DURING A SCHEMA MIGRATION ─────
# Seed a v1 file, migrate it to v2, and kill the process at each migration step.
# A fresh process must then recover the books — fully migrated, no data lost —
# regardless of where the crash landed.
echo
echo "== cross-process crash recovery during schema migration =="
migrate_crash_case() {
  point="$1"
  dir="$BUILD/.ptest_mig_$point"
  rm -rf "$dir"; mkdir -p "$dir"
  d="$(win "$PWD/$dir")"
  env ACCT_PTEST=migratewrite ACCT_CRASH_POINT="$point" ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
  wrote=$?
  vout=$(env ACCT_PTEST=migrateverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
  vstatus=$?
  if [ "$wrote" -eq 99 ] && [ "$vstatus" -eq 0 ]; then
    echo "  [PASS] migrate-crash@$point: killed (99), books recovered + migrated"
  else
    echo "  [FAIL] migrate-crash@$point: writer=$wrote verify=$vstatus  ($vout)"
    fail=1
  fi
}

migrate_crash_case afterMigrationTmp     # temp written, original intact      → re-migrate
migrate_crash_case afterMigrationBackup  # original→.bak, temp not installed  → finish forward
migrate_crash_case afterMigrationRename  # migrated in place, .bak not dropped → clean up

# ── Layer 2c: crash recovery in the AUDIT JOURNAL (event ↔ projection windows) ─
# Record an authoritative event, then kill the process in each window between the
# committed event and its projection/cursor. A fresh process must reconcile so the
# projection ends up EXACTLY at the committed log — never diverged or orphaned.
echo
echo "== cross-process crash recovery in the audit journal =="
aj_crash_case() {
  point="$1"
  dir="$BUILD/.ptest_aj_$point"
  rm -rf "$dir"; mkdir -p "$dir"
  d="$(win "$PWD/$dir")"
  env ACCT_PTEST=ajwrite ACCT_CRASH_POINT="$point" ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
  wrote=$?
  vout=$(env ACCT_PTEST=ajverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
  vstatus=$?
  if [ "$wrote" -eq 99 ] && [ "$vstatus" -eq 0 ]; then
    echo "  [PASS] aj-crash@$point: killed (99), projection reconciled to the log"
  else
    echo "  [FAIL] aj-crash@$point: writer=$wrote verify=$vstatus  ($vout)"
    fail=1
  fi
}

aj_crash_case afterEventFrame          # event durable-not-committed → rolled back to 3
aj_crash_case afterEventCommit         # event committed, not projected → reconcile to 4
aj_crash_case afterEventBeforeProject  # same window, explicit          → reconcile to 4
aj_crash_case afterProjectBeforeCursor # projected, cursor behind       → idempotent reconcile to 4

# ── Layer 2d: crash DURING projection verification must not touch authority ──
# Verification rebuilds into a scratch projection. Killing it mid-reconstruction
# must leave the live projection + log untouched, and a fresh verify() must pass.
echo
echo "== cross-process crash during projection verification =="
dir="$BUILD/.ptest_verify"; rm -rf "$dir"; mkdir -p "$dir"; d="$(win "$PWD/$dir")"
env ACCT_PTEST=verifywrite ACCT_CRASH_POINT=duringReconstruct ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
vw=$?
vout=$(env ACCT_PTEST=verifyverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
vv=$?
if [ "$vw" -eq 99 ] && [ "$vv" -eq 0 ]; then
  echo "  [PASS] verify-crash@duringReconstruct: killed (99), authority intact + verify passes"
else
  echo "  [FAIL] verify-crash: writer=$vw verify=$vv  ($vout)"
  fail=1
fi

# ── Layer 2e: crash DURING a period close must leave it atomic ────────────────
echo
echo "== cross-process crash during period close =="
pclose_crash_case() {
  point="$1"
  dir="$BUILD/.ptest_pclose_$point"; rm -rf "$dir"; mkdir -p "$dir"; d="$(win "$PWD/$dir")"
  env ACCT_PTEST=pclosewrite ACCT_CRASH_POINT="$point" ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
  pw=$?
  pout=$(env ACCT_PTEST=pcloseverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
  pv=$?
  if [ "$pw" -eq 99 ] && [ "$pv" -eq 0 ]; then
    echo "  [PASS] pclose-crash@$point: killed (99), close atomic + books intact"
  else
    echo "  [FAIL] pclose-crash@$point: writer=$pw verify=$pv  ($pout)"
    fail=1
  fi
}
pclose_crash_case afterEventFrame   # close event durable-not-committed → rolled back (not closed)
pclose_crash_case afterEventCommit  # close event committed → index rebuilds as closed on reopen

# ── Layer 2f: crash DURING a void correction must stay atomic ─────────────────
echo
echo "== cross-process crash during void correction =="
cr_crash_case() {
  point="$1"
  dir="$BUILD/.ptest_cr_$point"; rm -rf "$dir"; mkdir -p "$dir"; d="$(win "$PWD/$dir")"
  env ACCT_PTEST=crwrite ACCT_CRASH_POINT="$point" ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
  cw=$?
  cout=$(env ACCT_PTEST=crverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
  cv=$?
  if [ "$cw" -eq 99 ] && [ "$cv" -eq 0 ]; then
    echo "  [PASS] void-crash@$point: killed (99), status+lineage agree (atomic correction)"
  else
    echo "  [FAIL] void-crash@$point: writer=$cw verify=$cv  ($cout)"
    fail=1
  fi
}
cr_crash_case afterEventBeforeProject   # void committed, status not projected → reconcile heals
cr_crash_case afterProjectBeforeCursor  # status projected, cursor behind     → idempotent reconcile

# ── Layer 2g: crash DURING a payment allocation must stay atomic ──────────────
echo
echo "== cross-process crash during payment allocation =="
alloc_crash_case() {
  point="$1"
  dir="$BUILD/.ptest_alloc_$point"; rm -rf "$dir"; mkdir -p "$dir"; d="$(win "$PWD/$dir")"
  env ACCT_PTEST=allocwrite ACCT_CRASH_POINT="$point" ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
  aw=$?
  aout=$(env ACCT_PTEST=allocverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
  av=$?
  if [ "$aw" -eq 99 ] && [ "$av" -eq 0 ]; then
    echo "  [PASS] alloc-crash@$point: killed (99), balance consistent (atomic settlement)"
  else
    echo "  [FAIL] alloc-crash@$point: writer=$aw verify=$av  ($aout)"
    fail=1
  fi
}
alloc_crash_case afterEventBeforeProject   # allocation committed, index behind → reconcile heals
alloc_crash_case afterProjectBeforeCursor  # cursor behind → idempotent reconcile

# ── Layer 2h: crash DURING a ledger posting must stay atomic + balanced ───────
echo
echo "== cross-process crash during ledger posting =="
ledger_crash_case() {
  point="$1"
  dir="$BUILD/.ptest_ledger_$point"; rm -rf "$dir"; mkdir -p "$dir"; d="$(win "$PWD/$dir")"
  env ACCT_PTEST=ledgerwrite ACCT_CRASH_POINT="$point" ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
  lw=$?
  lout=$(env ACCT_PTEST=ledgerverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
  lv=$?
  if [ "$lw" -eq 99 ] && [ "$lv" -eq 0 ]; then
    echo "  [PASS] ledger-crash@$point: killed (99), trial balance 0 (atomic balanced posting)"
  else
    echo "  [FAIL] ledger-crash@$point: writer=$lw verify=$lv  ($lout)"
    fail=1
  fi
}
ledger_crash_case afterEventBeforeProject   # entry committed, index/cursor behind → reconcile heals
                                            # (ledger is index-only — no separate project step / 2nd window)

# ── Layer 2i: crash DURING snapshot creation must never corrupt authority ─────
echo
echo "== cross-process crash during snapshot creation =="
dir="$BUILD/.ptest_snap"; rm -rf "$dir"; mkdir -p "$dir"; d="$(win "$PWD/$dir")"
env ACCT_PTEST=snapwrite ACCT_CRASH_POINT=afterSnapshotTmp ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
sw=$?
sout=$(env ACCT_PTEST=snapverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
sv=$?
if [ "$sw" -eq 99 ] && [ "$sv" -eq 0 ]; then
  echo "  [PASS] snap-crash@afterSnapshotTmp: killed (99), snapshot complete-or-absent, genesis intact"
else
  echo "  [FAIL] snap-crash: writer=$sw verify=$sv  ($sout)"
  fail=1
fi

# ── Layer 2k: crash DURING a supplier event commit must stay atomic ───────────
# Suppliers are now event-authored (Full Domain Cutover). Killing the process between
# the committed event and its projection/cursor must reconcile to EXACTLY the log.
echo
echo "== cross-process crash during supplier event commit =="
sup_crash_case() {
  point="$1"
  dir="$BUILD/.ptest_sup_$point"; rm -rf "$dir"; mkdir -p "$dir"; d="$(win "$PWD/$dir")"
  env ACCT_PTEST=supwrite ACCT_CRASH_POINT="$point" ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
  sw=$?
  sout=$(env ACCT_PTEST=supverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
  sv=$?
  if [ "$sw" -eq 99 ] && [ "$sv" -eq 0 ]; then
    echo "  [PASS] sup-crash@$point: killed (99), supplier projection reconciled to the log"
  else
    echo "  [FAIL] sup-crash@$point: writer=$sw verify=$sv  ($sout)"
    fail=1
  fi
}
sup_crash_case afterEventBeforeProject   # event committed, not projected → reconcile heals
sup_crash_case afterProjectBeforeCursor  # projected, cursor behind       → idempotent reconcile

# ── Layer 2l: crash DURING an invoice event commit must stay atomic ───────────
# Invoices are now event-authored via the editor cutover. A kill mid-commit leaves the
# invoice + its line fully present (total == Σ lines) or fully absent — never partial.
echo
echo "== cross-process crash during invoice event commit =="
inv_crash_case() {
  point="$1"
  dir="$BUILD/.ptest_inv_$point"; rm -rf "$dir"; mkdir -p "$dir"; d="$(win "$PWD/$dir")"
  env ACCT_PTEST=invwrite ACCT_CRASH_POINT="$point" ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
  iw=$?
  iout=$(env ACCT_PTEST=invverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
  iv=$?
  if [ "$iw" -eq 99 ] && [ "$iv" -eq 0 ]; then
    echo "  [PASS] inv-crash@$point: killed (99), invoice+line reconciled (total == Σ lines)"
  else
    echo "  [FAIL] inv-crash@$point: writer=$iw verify=$iv  ($iout)"
    fail=1
  fi
}
inv_crash_case afterEventBeforeProject   # event committed, not projected → reconcile heals
inv_crash_case afterProjectBeforeCursor  # projected, cursor behind       → idempotent reconcile

# ── Layer 2m: crash DURING an atomic business transaction (invoice + ledger posting) ──
# The invoice event and its ledger revenue posting are ONE fact (EventLog::appendAtomic).
# Every interruption must leave the books in exactly one of two observable states: the whole
# transaction ABSENT, or COMPLETELY represented (invoice AND posting, trial balance 0) —
# never a split (operational fact without its financial interpretation, or vice versa).
echo
echo "== cross-process crash during an atomic business transaction =="
txn_crash_case() {
  point="$1"
  dir="$BUILD/.ptest_txn_$point"; rm -rf "$dir"; mkdir -p "$dir"; d="$(win "$PWD/$dir")"
  env ACCT_PTEST=txnwrite ACCT_CRASH_POINT="$point" ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
  tw=$?
  tout=$(env ACCT_PTEST=txnverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
  tv=$?
  if [ "$tw" -eq 99 ] && [ "$tv" -eq 0 ]; then
    echo "  [PASS] txn-crash@$point: killed (99), books absent-or-complete (never split)"
  else
    echo "  [FAIL] txn-crash@$point: writer=$tw verify=$tv  ($tout)"
    fail=1
  fi
}
txn_crash_case afterTxnFirstFrame        # first event durable, group NOT committed → ABSENT
txn_crash_case afterTxnCommit            # group committed, not projected → reconcile → COMPLETE
txn_crash_case afterProjectBeforeCursor  # projected, cursor behind → idempotent reconcile → COMPLETE

# ── Layer 2j: crash DURING a compatibility-manifest write must stay atomic ────
# The compat.manifest is a disposable PROJECTION of the authoritative
# EngineVersionStamp events. A crash between its durable temp and the atomic install
# must leave it complete-or-absent — never partial. The governance stamp survives in
# the log, so a fresh process rebuilds/validates the contract unambiguously.
echo
echo "== cross-process crash during compatibility-manifest write =="
dir="$BUILD/.ptest_compat"; rm -rf "$dir"; mkdir -p "$dir"; d="$(win "$PWD/$dir")"
env ACCT_PTEST=compatwrite ACCT_CRASH_POINT=afterManifestTmp ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
cw=$?
cout=$(env ACCT_PTEST=compatverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
cv=$?
if [ "$cw" -eq 99 ] && [ "$cv" -eq 0 ]; then
  echo "  [PASS] compat-crash@afterManifestTmp: killed (99), manifest complete-or-absent, governance intact"
else
  echo "  [FAIL] compat-crash: writer=$cw verify=$cv  ($cout)"
  fail=1
fi

# Baseline: a clean (uncrashed) write must also verify, proving the test isn't
# trivially passing because every reopen "recovers".
echo
echo "== clean-write baseline =="
bdir="$BUILD/.ptest_clean"; rm -rf "$bdir"; mkdir -p "$bdir"; bd="$(win "$PWD/$bdir")"
env ACCT_PTEST=crashwrite ACCT_DATA_DIR="$bd" "./$EXE" >/dev/null 2>&1
cw=$?
env ACCT_PTEST=crashverify ACCT_DATA_DIR="$bd" "./$EXE" >/dev/null 2>&1
cv=$?
if [ "$cw" -eq 0 ] && [ "$cv" -eq 0 ]; then
  echo "  [PASS] clean write completes (0) and verifies (0)"
else
  echo "  [FAIL] clean baseline: write=$cw verify=$cv"
  fail=1
fi

echo
if [ "$fail" -eq 0 ]; then
  echo "== ALL PERSISTENCE TESTS PASSED =="
else
  echo "== PERSISTENCE TESTS FAILED =="
fi
exit "$fail"
