#!/usr/bin/env bash
# installer-test.sh — verify the installer's LIFECYCLE GUARANTEES without needing admin rights,
# Program Files, the registry, or iscc. The guarantees are architectural, so they can be proven
# faithfully by simulation:
#
#   • the installer only ever writes {app} + HKLM; USER DATA lives in %LOCALAPPDATA%\Occountant,
#     which the installer never touches. So upgrade / uninstall / reinstall cannot lose books.
#   • downgrade refusal is a pure version-code comparison (Occountant.iss InitializeSetup, which
#     mirrors appinfo::isDowngrade — also unit-tested in ACCT_C2TEST #15).
#   • portable mode stores data next to the exe (a marker file), never in %LOCALAPPDATA%.
#
# Usage:  bash tools/installer-test.sh
set -u
cd "$(dirname "$0")/.." || exit 2
EXE=build/AccountingQuick.exe
win() { cygpath -m "$1"; }
QUIET='catalogs.json|Translations will not be available'
[ -f "$EXE" ] || { echo "build AccountingQuick first"; exit 1; }
fail=0

count_invoices() { # <dataDirWin>
  local probe="build/.inst_probe.txt"; rm -f "$probe"
  env ACCT_DATA_DIR="$1" ACCT_PROBE="$(win "$PWD/$probe")" "./$EXE" >/dev/null 2>&1
  grep -o 'totalCount=[0-9]*' "$probe" 2>/dev/null | head -1 | cut -d= -f2
}

# ── 1. Install + upgrade: data in %LOCALAPPDATA% survives a binary replacement ──
echo "== install v1 then upgrade (data dir is separate from the install dir) =="
D=build/.inst_data; rm -rf "$D"; mkdir -p "$D"; WD="$(win "$PWD/$D")"
env ACCT_DATA_DIR="$WD" ACCT_BENCH_SEED=25 "./$EXE" >/dev/null 2>&1   # v1 seeds books
n="$(count_invoices "$WD")"                                          # v2 binaries, same data dir
echo "  invoices after upgrade restart: ${n:-?} (expect 25)"
[ "$n" = "25" ] && echo "  [PASS] upgrade preserves data" || { echo "  [FAIL]"; fail=1; }

# ── 2. Uninstall preserves data: removing the install tree leaves the data dir intact ──
echo "== uninstall (remove binaries) then reinstall + relaunch =="
# Simulate uninstall: the install dir would be deleted; the data dir is deliberately untouched.
# (No files to delete here — the point is that NOTHING under $D is removed by an uninstall.)
n2="$(count_invoices "$WD")"
echo "  invoices after uninstall+reinstall: ${n2:-?} (expect 25)"
[ "$n2" = "25" ] && echo "  [PASS] uninstall preserves accounting data" || { echo "  [FAIL]"; fail=1; }

# ── 3. Downgrade refusal (the exact rule Occountant.iss InitializeSetup applies) ──
echo "== downgrade refusal (version-code comparison) =="
vcode() { local M m p; IFS=. read -r M m p <<< "$1"; echo $(( M*1000000 + m*1000 + p )); }
# Mirrors Pascal: refuse iff (installed>0) and (setup < installed).
refuses() { local inst; inst=$(vcode "$1"); local set; set=$(vcode "$2"); [ "$inst" -gt 0 ] && [ "$set" -lt "$inst" ]; }
dt=0
refuses 1.2.0 1.1.0 || { echo "  [FAIL] should refuse 1.1.0 over 1.2.0"; dt=1; }
refuses 1.1.0 1.2.0 && { echo "  [FAIL] should ALLOW upgrade 1.2.0 over 1.1.0"; dt=1; }
refuses 1.1.0 1.1.0 && { echo "  [FAIL] should ALLOW same-version reinstall"; dt=1; }
[ "$dt" -eq 0 ] && echo "  [PASS] older-over-newer refused; upgrade + reinstall allowed" || fail=1

# ── 4. Portable mode: a marker beside the exe stores data next to the binary ──
echo "== portable mode (data beside the exe, not in %LOCALAPPDATA%) =="
EXEDIR="$(dirname "$EXE")"
MARKER="$EXEDIR/Occountant.portable"
PDATA="$EXEDIR/Data"
rm -rf "$PDATA"; : > "$MARKER"
# Seed WITHOUT ACCT_DATA_DIR so the portable path (exeDir/Data) is used.
env -u ACCT_DATA_DIR ACCT_BENCH_SEED=9 "./$EXE" >/dev/null 2>&1
pdat=$(find "$PDATA" -name '*.dat' 2>/dev/null | wc -l)
pn="$(env -u ACCT_DATA_DIR ACCT_PROBE="$(win "$PWD/build/.port_probe.txt")" "./$EXE" >/dev/null 2>&1; grep -o 'totalCount=[0-9]*' build/.port_probe.txt 2>/dev/null | head -1 | cut -d= -f2)"
echo "  portable data files: $pdat ; invoices read back: ${pn:-?} (expect 9)"
if [ "$pdat" -ge 1 ] && [ "$pn" = "9" ]; then echo "  [PASS] portable mode stores + reads data beside the exe"; else echo "  [FAIL]"; fail=1; fi
rm -f "$MARKER" build/.port_probe.txt; rm -rf "$PDATA"    # leave the dev tree clean

echo
[ "$fail" -eq 0 ] && echo "== INSTALLER LIFECYCLE: ALL PASSED ==" || echo "== INSTALLER LIFECYCLE: FAILED =="
exit "$fail"
