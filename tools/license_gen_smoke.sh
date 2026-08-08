#!/usr/bin/env bash
# license_gen_smoke.sh — proves the vendor license generator RUNS and emits a valid OCCLIC key,
# including in a clean room (no MSYS2/ucrt64 on PATH). Guards the STATUS_DLL_NOT_FOUND (0xC0000135)
# regression: license_gen must ship its own DLL closure, not rely on build/ or the dev PATH.
#
# Usage: bash tools/license_gen_smoke.sh
# Exit 0 = generator built, self-contained, and produced a valid OCCLIC-… key with a clean PATH.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 2
BUILD="build"
DIST="$BUILD/license_gen_dist"

echo "== 1/3 build self-contained license_gen_dist =="
cmake --build "$BUILD" --target license_gen_dist >/dev/null 2>&1 || { echo "FAIL: build"; exit 1; }
[ -f "$DIST/license_gen.exe" ] || { echo "FAIL: $DIST/license_gen.exe missing"; exit 1; }
[ -f "$DIST/Qt6Core.dll" ]     || { echo "FAIL: Qt6Core.dll not bundled next to the tool"; exit 1; }
echo "  ok: $DIST/license_gen.exe + DLL closure present"

echo "== 2/3 clean-room run (PATH = Windows system only, no ucrt64) =="
# Strip PATH to just the Windows dirs so DLLs can ONLY resolve from the exe's own directory.
out="$(PATH="/c/Windows/System32:/c/Windows" \
      "./$DIST/license_gen.exe" --name "Test Shop" --plan Business --expires 2027-01-01 2>/dev/null)"
rc=$?
echo "  exit=$rc"
[ "$rc" -eq 0 ] || { echo "FAIL: generator did not run cleanly (rc=$rc; 0xC0000135 => missing DLL)"; exit 1; }

echo "== 3/3 verify a valid OCCLIC key was emitted =="
key="$(printf '%s\n' "$out" | grep '^OCCLIC-' | head -1)"
case "$key" in
  OCCLIC-*.* ) echo "  ok: ${key:0:32}…  (len=${#key})" ;;
  * ) echo "FAIL: no OCCLIC-<payload>.<sig> line in output"; printf '%s\n' "$out"; exit 1 ;;
esac

echo "== license_gen smoke: PASSED =="
