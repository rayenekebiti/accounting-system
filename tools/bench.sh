#!/usr/bin/env bash
# bench.sh — performance benchmark matrix. Builds the app, then for each dataset
# size seeds an ISOLATED dir and measures real load/aggregate/filter/editor/memory
# timings (QElapsedTimer; working set via psapi). Never touches real user data.
#
# Usage:   bash tools/bench.sh [sizes...]   (default: 10 1000 10000)

set -u
cd "$(dirname "$0")/.." || exit 2
BUILD=build
EXE="$BUILD/AccountingQuick.exe"
win() { cygpath -m "$1"; }
SIZES=("${@:-10 1000 10000}")

echo "== building =="
cmake --build "$BUILD" --target AccountingQuick 2>&1 | grep -iE "error:|error C" && { echo "BUILD FAILED"; exit 1; }

for N in ${SIZES[@]}; do
  DIR="$BUILD/.bench_$N"; rm -rf "$DIR"; mkdir -p "$DIR"; W=$(win "$PWD/$DIR")
  echo ""
  echo "######## dataset: $N invoices ########"
  env ACCT_DATA_DIR="$W" ACCT_BENCH_SEED="$N" "./$EXE" 2>/dev/null
  env ACCT_DATA_DIR="$W" ACCT_BENCH=1 "./$EXE" 2>&1 \
    | grep -vE "catalogs.json|Translations will not|loaded .* invoices"
done

# Memory characterization: long open/close run (set ACCT_BENCH_CYCLES).
echo ""
echo "######## memory: 1000 editor cycles (10k dataset) ########"
W=$(win "$PWD/$BUILD/.bench_10000")
[ -d "$BUILD/.bench_10000" ] && env ACCT_DATA_DIR="$W" ACCT_BENCH=1 ACCT_BENCH_CYCLES=1000 "./$EXE" 2>&1 \
  | grep -E "editor open/close|working set"
