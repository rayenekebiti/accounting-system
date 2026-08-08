#!/usr/bin/env bash
# shots.sh — screenshot regression harness.
# Builds the app, seeds an ISOLATED deterministic dataset (never touches the
# user's real books), and captures every screen + both editors in EN, FR, AR at
# standard and minimum window sizes. Output baselines go to build/baselines/<lang>/<size>/.
#
# Usage:   bash tools/shots.sh
# Then visually diff build/baselines/ against approved baselines for RTL/i18n regressions.

set -u
cd "$(dirname "$0")/.." || exit 2
BUILD=build
EXE="$BUILD/AccountingQuick.exe"
DATA="$BUILD/.shotdata"             # isolated dataset (git-ignored)
OUT="$BUILD/baselines"

win() { cygpath -m "$1"; }          # MSYS path -> Windows path (QImage::save needs it)

echo "== building =="
cmake --build "$BUILD" --target AccountingQuick 2>&1 | grep -iE "error:|error C" && { echo "BUILD FAILED"; exit 1; }
[ -f "$EXE" ] || { echo "missing $EXE"; exit 1; }

echo "== seeding isolated dataset: $DATA =="
rm -rf "$DATA"; mkdir -p "$DATA"
ACCT_DATA_DIR="$(win "$PWD/$DATA")" ACCT_SEED=1 "./$EXE" 2>/dev/null

echo "== capturing EN / FR / AR  x  standard / min =="
for lang in en fr ar; do
  for size in "standard:" "min:820:560"; do
    name="${size%%:*}"; dims="${size#*:}"
    dir="$OUT/$lang/$name"; rm -rf "$dir"; mkdir -p "$dir"
    size_env=()
    if [ -n "$dims" ]; then size_env=(ACCT_SHOT_W="${dims%%:*}" ACCT_SHOT_H="${dims#*:}"); fi
    echo "  - $lang / $name"
    # Use `env` (not a bare VAR=val prefix): bash does NOT treat assignments
    # produced by expansion as env assignments, but `env` parses its args as such.
    env ACCT_DATA_DIR="$(win "$PWD/$DATA")" \
        ACCT_SHOT="$(win "$PWD/$dir")" \
        ACCT_SHOT_LANG="$lang" \
        "${size_env[@]}" \
        "./$EXE" 2>/dev/null
  done
done

echo "== done. baselines in $OUT/<lang>/<size>/ =="
find "$OUT" -name "*.png" | sort | sed 's,^,  ,'
