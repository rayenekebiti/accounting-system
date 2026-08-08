#!/usr/bin/env bash
# config-resilience.sh — corrupt every NON-ACCOUNTING config artifact and prove the app still
# starts and the books stay fully intact. (Corrupting the ACCOUNTING data files is a different
# guarantee — the engine's refuse-or-recover — and is covered by tools/fuzz.sh + tools/ptest.sh.)
#
# The commercial/UX config layer must DEGRADE GRACEFULLY: a garbled settings file, license cache,
# update manifest, backup index, or log must never crash, hang, or lose data — the app falls back
# to defaults / ignores the artifact and the accounting data is untouched.
#
# Runs in PORTABLE mode (a marker beside the exe) so every config artifact is a real file under
# <exeDir>/Data — isolable and corruptible. The dev tree is restored on exit.
#
# Usage:  bash tools/config-resilience.sh
set -u
cd "$(dirname "$0")/.." || exit 2
EXE=build/AccountingQuick.exe
win() { cygpath -m "$1"; }
[ -f "$EXE" ] || { echo "build AccountingQuick first"; exit 1; }

EXEDIR="$(dirname "$EXE")"
MARKER="$EXEDIR/Occountant.portable"
DATA="$EXEDIR/Data"
SEED=12
fail=0
cleanup() { rm -f "$MARKER" build/.cfg_probe.txt; rm -rf "$DATA"; }
trap cleanup EXIT

: > "$MARKER"                      # enable portable mode (data + config beside the exe)
rm -rf "$DATA"

echo "== seed a clean portable install ($SEED invoices) =="
env -u ACCT_DATA_DIR ACCT_BENCH_SEED=$SEED "./$EXE" >/dev/null 2>&1
# Warm the config layer: a normal startup writes settings + issues a trial license.
env -u ACCT_DATA_DIR ACCT_PROBE="$(win "$PWD/build/.cfg_probe.txt")" "./$EXE" >/dev/null 2>&1
SNAP="$EXEDIR/.Data.snapshot"; rm -rf "$SNAP"; cp -r "$DATA" "$SNAP"

# Locate the artifacts (paths are discovered, not assumed, so a layout change can't silently skip).
INI="$(find "$DATA/config" -name '*.ini' 2>/dev/null | head -1)"
LICKEY="$(find "$DATA/config" -name 'license.key' 2>/dev/null | head -1)"
LOG="$(find "$DATA/logs" -name 'occountant.log' 2>/dev/null | head -1)"
echo "  settings ini: ${INI:-<none>}"
echo "  license key : ${LICKEY:-<none>}"
echo "  log         : ${LOG:-<none>}"

# Fabricate the artifacts a bare seed doesn't create, so their corrupt-handling is still exercised:
#   • an update source manifest (garbage), and a corrupt staged/pending update
#   • a corrupt backup restore point
mkdir -p "$DATA/config/updates/source" "$DATA/config/updates/.pending-update" "$DATA/backups/backup-1"

# probe_intact <label> — start the app (portable) and assert the books are FULLY intact.
# RECOVERED = the probe wrote totalCount==SEED. Anything else (crash / hang / data loss) FAILS.
probe_intact() {
  local label="$1"; local pf="build/.cfg_probe.txt"; rm -f "$pf"
  timeout 90 env -u ACCT_DATA_DIR ACCT_PROBE="$(win "$PWD/$pf")" "./$EXE" >/dev/null 2>&1
  local rc=$?
  local n; n="$(grep -o 'totalCount=[0-9]*' "$pf" 2>/dev/null | head -1 | cut -d= -f2)"
  if [ "$rc" = "124" ]; then echo "  [FAIL] $label: startup HUNG (timeout)"; fail=1; return; fi
  if [ "${n:-x}" = "$SEED" ]; then echo "  [PASS] $label: started, books intact ($n invoices)";
  else echo "  [FAIL] $label: rc=$rc, books=${n:-<none>} (expected $SEED)"; fail=1; fi
}

restore_all() { rm -rf "$DATA"; cp -r "$SNAP" "$DATA"
  mkdir -p "$DATA/config/updates/source" "$DATA/config/updates/.pending-update" "$DATA/backups/backup-1"; }

# garbage <path> — overwrite with binary garbage (create parent if needed).
garbage() { mkdir -p "$(dirname "$1")"; printf '\x00\xff\x01\xfe garbage \x00\x00 not-valid }{[' > "$1"; }

echo
echo "== baseline: a clean start reports all $SEED invoices =="
probe_intact "baseline (uncorrupted)"

echo
echo "== corrupt each NON-ACCOUNTING config artifact; books must survive every time =="

# 1. Settings / preferences (QSettings ini) — garbage bytes.
if [ -n "$INI" ]; then garbage "$INI"; probe_intact "settings: garbage ini"; restore_all
  # 1b. Settings — truncated to empty.
  : > "$INI"; probe_intact "settings: empty ini"; restore_all
  # 1c. Settings — hostile values (5k-char currency, NUL-laden channel).
  printf '[General]\ncurrency=%0.s#' {1..5000} > "$INI"; printf '\nupdate\\channel=\x00\x00bogus\n' >> "$INI"
  probe_intact "settings: hostile values"; restore_all
else echo "  [skip] no settings ini produced"; fi

# 2. License cache + key.
if [ -n "$LICKEY" ]; then garbage "$LICKEY"; probe_intact "license: garbage key"; restore_all; fi
garbage "$DATA/config/license.cache"; probe_intact "license: garbage cache"; restore_all

# 3. Update cache — garbage manifest + a corrupt pending (staged) update.
garbage "$DATA/config/updates/source/manifest.json"; probe_intact "update: garbage source manifest"; restore_all
garbage "$DATA/config/updates/.pending-update/manifest.json"
garbage "$DATA/config/updates/.pending-update/Occountant-setup.bin"
probe_intact "update: corrupt pending/staged bundle"; restore_all

# 4. Backup history — a corrupt restore point (authoritative log garbled).
garbage "$DATA/backups/backup-1/audit.log"; garbage "$DATA/backups/backup-1/customers.dat"
probe_intact "backup: corrupt restore point"; restore_all

# 5. Diagnostics / operator log.
if [ -n "$LOG" ]; then garbage "$LOG"; probe_intact "log: garbage occountant.log"; restore_all; fi

# 6. A stray restore/pending pointer with NO staged copy (interrupted restore intent).
if [ -n "$INI" ]; then printf '\n[restore]\npending=nonexistent-backup\n' >> "$INI"
  probe_intact "settings: dangling restore/pending pointer"; restore_all; fi

rm -rf "$SNAP"
echo
[ "$fail" -eq 0 ] && echo "== CONFIG RESILIENCE: ALL PASSED (every corrupt artifact degraded gracefully) ==" \
                  || echo "== CONFIG RESILIENCE: FAILED =="
exit "$fail"
