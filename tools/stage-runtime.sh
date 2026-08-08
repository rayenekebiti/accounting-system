#!/usr/bin/env bash
# stage-runtime.sh — assemble a CLEAN, clean-machine-deployable Occountant runtime tree.
#
# Produces a staging directory containing ONLY what ships: the exe, the Qt runtime + plugins + qml
# modules, the MSYS2 third-party DLL closure, and an embedded BuildInfo.json. No dev junk
# (*_autogen, CMake files, .obj, test scratch, baselines, the Widgets exe). Both the Inno installer
# and the portable ZIP are built over this same tree, so they are byte-for-byte the same payload.
#
# Usage:  bash tools/stage-runtime.sh [build-dir] [stage-dir]
# Output: <stage-dir> (default dist/Occountant)
set -u
cd "$(dirname "$0")/.." || exit 2
BUILD="${1:-build}"
STAGE="${2:-dist/Occountant}"
EXE="$BUILD/AccountingQuick.exe"
win() { cygpath -m "$1"; }

[ -f "$EXE" ] || { echo "stage-runtime: build AccountingQuick first ($EXE missing)"; exit 1; }

echo "== completing MSYS2 third-party DLL closure =="
bash tools/deploy-deps.sh "$BUILD" | tail -1

echo "== staging clean runtime tree (allowlist) into $STAGE =="
rm -rf "$STAGE"; mkdir -p "$STAGE"
cp "$EXE" "$STAGE/"
cp "$BUILD"/*.dll "$STAGE/" 2>/dev/null
for d in platforms styles imageformats tls generic networkinformation qml; do
  [ -d "$BUILD/$d" ] && cp -r "$BUILD/$d" "$STAGE/"
done

# Embed the deterministic BuildInfo.json next to the binary (the release manifest + support
# tooling read it; the About screen can fall back to it). Uses the just-built exe headlessly.
env ACCT_DATA_DIR="$(win "$PWD/$BUILD/.stage_bi")" \
    ACCT_BUILDINFO="$(win "$PWD/$STAGE/BuildInfo.json")" "./$EXE" >/dev/null 2>&1
rm -rf "$BUILD/.stage_bi"

files=$(find "$STAGE" -type f | wc -l)
size=$(du -sh "$STAGE" | cut -f1)
echo "  staged $files files ($size); BuildInfo.json embedded"
[ -f "$STAGE/BuildInfo.json" ] || { echo "stage-runtime: BuildInfo.json was not emitted"; exit 1; }
[ -f "$STAGE/AccountingQuick.exe" ] || { echo "stage-runtime: exe missing from stage"; exit 1; }
