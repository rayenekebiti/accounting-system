#!/usr/bin/env bash
# upgrade-test.sh — verify upgrade-safety WITHOUT touching Program Files / the registry.
#
# The real upgrade risk is data loss, and the architecture's answer is that user data
# lives in %LOCALAPPDATA% (here: an isolated ACCT_DATA_DIR), entirely separate from the
# install dir. An "upgrade" is therefore just: new binaries + restart against the SAME
# data dir. This script proves the data survives that, including a crash-leftover
# journal being recovered by the post-upgrade launch.
#
# (The installer itself, tools/installer.nsi, only ever writes $INSTDIR + HKLM — it
# never references the data dir — so this simulation is faithful to what an install does.)
#
# Usage:  bash tools/upgrade-test.sh
set -u
cd "$(dirname "$0")/.." || exit 2
EXE=build/AccountingQuick.exe
D=build/.upgrade_userdata
win() { cygpath -m "$1"; }
[ -f "$EXE" ] || { echo "build AccountingQuick first"; exit 1; }
rm -rf "$D"; mkdir -p "$D"; WD="$(win "$PWD/$D")"
fail=0

echo "== v1 launch: seed 25 invoices into user data =="
env ACCT_DATA_DIR="$WD" ACCT_BENCH_SEED=25 "./$EXE" >/dev/null 2>&1

count_invoices() {
  local probe="$D/probe.txt"; rm -f "$probe"
  env ACCT_DATA_DIR="$WD" ACCT_PROBE="$(win "$PWD/$probe")" "./$EXE" >/dev/null 2>&1
  grep -o 'totalCount=[0-9]*' "$probe" 2>/dev/null | head -1 | cut -d= -f2
}

echo "== simulate upgrade: relaunch (new binaries) against the SAME data dir =="
n="$(count_invoices)"
echo "  invoices visible after upgrade restart: ${n:-?} (expect 25)"
[ "$n" = "25" ] && echo "  [PASS] persistence preserved across upgrade" || { echo "  [FAIL]"; fail=1; }

echo "== crash mid-save at upgrade time, then relaunch with new binaries =="
env ACCT_DATA_DIR="$WD" ACCT_PTEST=crashwrite ACCT_CRASH_POINT=afterMainWrite "./$EXE" >/dev/null 2>&1
wrote=$?
ver="$(env ACCT_DATA_DIR="$WD" ACCT_PTEST=crashverify "./$EXE" 2>&1 | grep -i sentinel)"
echo "  writer exit=$wrote; $ver"
echo "$ver" | grep -q PRESENT && echo "  [PASS] journal recovered after upgrade restart" || { echo "  [FAIL]"; fail=1; }

echo "== re-verify the 25 invoices are STILL intact after the recovery cycle =="
n2="$(count_invoices)"
echo "  invoices: ${n2:-?} (expect 25)"
[ "$n2" = "25" ] && echo "  [PASS] books intact after crash+recovery" || { echo "  [FAIL]"; fail=1; }

echo
[ "$fail" -eq 0 ] && echo "== UPGRADE-SAFETY: ALL PASSED ==" || echo "== UPGRADE-SAFETY: FAILED =="
exit "$fail"
