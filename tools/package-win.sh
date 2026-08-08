#!/usr/bin/env bash
# package-win.sh — produce a distributable Windows installer via the LEGACY NSIS path.
#
# DEPRECATED: the shipping pipeline is tools/release.sh (Inno installer + portable ZIP + manifests
# + signing + checksums, one command). This script is retained only as an NSIS fallback for
# environments with makensis but not iscc.
#
# Pipeline:
#   1. complete the MSYS2 third-party DLL closure (deploy-deps.sh)
#   2. stage a CLEAN runtime tree by allowlist (no dev junk: no *_autogen, CMake
#      files, .obj, test scratch, the widgets exe, baselines, qmltooling, …)
#   3. build the NSIS installer over the staged tree
#
# Usage:  bash tools/package-win.sh [version]
# Output: dist/Occountant-<version>-Setup.exe

set -u
cd "$(dirname "$0")/.." || exit 2
BUILD=build
STAGE=dist/Occountant
VERSION="${1:-1.0.0}"
MAKENSIS="/c/msys64/mingw32/bin/makensis.exe"
win() { cygpath -w "$1"; }

[ -f "$BUILD/AccountingQuick.exe" ] || { echo "build AccountingQuick first (cmake --build $BUILD --target AccountingQuick)"; exit 1; }
[ -x "$MAKENSIS" ] || { echo "makensis not found at $MAKENSIS (pacman -S mingw-w64-i686-nsis)"; exit 1; }

echo "== completing DLL closure =="
bash tools/deploy-deps.sh "$BUILD" | tail -1

echo "== staging clean runtime tree (allowlist) =="
rm -rf "$STAGE"; mkdir -p "$STAGE"
cp "$BUILD/AccountingQuick.exe" "$STAGE/"
cp "$BUILD"/*.dll "$STAGE/" 2>/dev/null
for d in platforms styles imageformats tls generic networkinformation qml; do
  [ -d "$BUILD/$d" ] && cp -r "$BUILD/$d" "$STAGE/"
done
files=$(find "$STAGE" -type f | wc -l)
size=$(du -sh "$STAGE" | cut -f1)
echo "  staged $files files ($size)"

echo "== building installer (NSIS) =="
mkdir -p dist
OUT="dist/Occountant-$VERSION-Setup.exe"
rm -f "$OUT"
"$MAKENSIS" -V2 \
  -DVERSION="$VERSION" \
  -DSTAGE="$(win "$PWD/$STAGE")" \
  -DOUTFILE="$(win "$PWD/$OUT")" \
  tools/installer.nsi 2>&1 | tail -8

if [ -f "$OUT" ]; then
  echo "  OK installer: $OUT ($(du -h "$OUT" | cut -f1))"
else
  echo "  INSTALLER BUILD FAILED"; exit 1
fi
