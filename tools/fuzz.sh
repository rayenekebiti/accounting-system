#!/usr/bin/env bash
# fuzz.sh — adversarial robustness gate (deterministic, isolated).
#
# Two layers:
#   1. In-process fuzz + property suite (ACCT_FUZZ=suite): structure-aware byte-mutation
#      fuzzers over every on-disk boundary (EventLog frames, ledger snapshot, compat
#      manifest, BinaryRecordFile, classify) + property-based invariants over randomized
#      valid histories. Deterministic + seeded (reproducible). Depth via ITERS / SEED.
#   2. REAL cross-process fault injection: build valid state, arm a persistence write to
#      fail (ACCT_FAULT_ARM), then a fresh process must recover — reconcile / uncommitted-
#      tail truncation / manifest rebuild / snapshot genesis-fallback.
#
# Usage:  bash tools/fuzz.sh            # fast default (CI-friendly)
#         ITERS=50000 bash tools/fuzz.sh   # deep run
#         SEED=0x1234 bash tools/fuzz.sh   # reproduce a specific run
# Exit 0 = robust under hostile conditions.

set -u
cd "$(dirname "$0")/.." || exit 2
BUILD=build
EXE="$BUILD/AccountingQuick.exe"
win() { cygpath -m "$1"; }
QUIET='catalogs.json|Translations will not be available'
ITERS="${ITERS:-400}"
SEED="${SEED:-}"

echo "== building =="
cmake --build "$BUILD" --target AccountingQuick 2>&1 | grep -iE "error:|error C" && { echo "BUILD FAILED"; exit 1; }
[ -f "$EXE" ] || { echo "missing $EXE"; exit 1; }

fail=0

# ── Layer 1: in-process fuzz + property suite ────────────────────────────────
echo
echo "== fuzz + property suite (ITERS=$ITERS) =="
SUITE_DIR="$BUILD/.fuzz_suite"; rm -rf "$SUITE_DIR"; mkdir -p "$SUITE_DIR"
env ACCT_FUZZ=suite ACCT_FUZZ_ITERS="$ITERS" ${SEED:+ACCT_FUZZ_SEED="$SEED"} \
    ACCT_DATA_DIR="$(win "$PWD/$SUITE_DIR")" "./$EXE" 2>&1 | grep -vE "$QUIET"
suite_status=${PIPESTATUS[0]}
[ "$suite_status" -eq 0 ] || { echo "  -> fuzz suite FAILED ($suite_status)"; fail=1; }

# ── Layer 2: cross-process fault injection (prove recovery) ──────────────────
# For each persistence write point: injure it in one process, then a fresh process must
# reopen and recover (verifyAll clean, trial balance 0, snapshot valid-or-absent, governance
# intact) — whether the transaction ended up reconciled-present or cleanly-absent.
echo
echo "== cross-process fault injection (recovery) =="
fault_case() {
  point="$1"
  dir="$BUILD/.fuzz_fault_$point"; rm -rf "$dir"; mkdir -p "$dir"; d="$(win "$PWD/$dir")"
  env ACCT_FAULT_ARM="$point" ACCT_FUZZ=faultwrite ACCT_DATA_DIR="$d" "./$EXE" >/dev/null 2>&1
  fw=$?
  fout=$(env ACCT_FUZZ=faultverify ACCT_DATA_DIR="$d" "./$EXE" 2>&1 | grep -vE "$QUIET")
  fv=$?
  if [ "$fw" -eq 0 ] && [ "$fv" -eq 0 ]; then
    echo "  [PASS] fault@$point: writer survived, fresh process recovered"
  else
    echo "  [FAIL] fault@$point: writer=$fw verify=$fv  ($fout)"
    fail=1
  fi
}
fault_case logCommit      # commit-point header write fails → group cleanly absent (truncated)
fault_case cursorWrite    # cursor advance fails → reconcile heals the projection to the log
fault_case snapshotWrite  # snapshot install fails → genesis fallback (snapshot absent)
fault_case manifestWrite  # manifest write fails → rebuilt from EngineVersionStamp events

echo
if [ "$fail" -eq 0 ]; then
  echo "== ROBUST: all adversarial + fault-injection checks passed =="
else
  echo "== ROBUSTNESS FAILURES DETECTED =="
fi
exit "$fail"
