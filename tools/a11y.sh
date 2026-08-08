#!/usr/bin/env bash
# a11y.sh — accessibility gate. Walks the REALIZED accessible tree and asserts every VISIBLE
# interactive control (button, text field, combo, tab, …) exposes an accessible name, both with an
# empty company (empty states) and with seeded data (list rows). Screen-reader / high-contrast /
# large-font behaviour beyond the tree is a documented manual pass.
#
# Usage:  bash tools/a11y.sh
set -u
cd "$(dirname "$0")/.." || exit 2
BUILD=build
EXE="$BUILD/AccountingQuick.exe"
win() { cygpath -m "$1"; }
QUIET='catalogs.json|Translations will not be available'
fail=0

echo "== building =="
cmake --build "$BUILD" --target AccountingQuick 2>&1 | grep -iE "error:|error C" && { echo "BUILD FAILED"; exit 1; }

run_a11y() { # <label> <seed>
  local label="$1" seed="$2"
  local d="$BUILD/.a11y_$label"; rm -rf "$d"; mkdir -p "$d"
  [ "$seed" -gt 0 ] && env ACCT_DATA_DIR="$(win "$PWD/$d")" ACCT_BENCH_SEED="$seed" "./$EXE" >/dev/null 2>&1
  local out; out=$(timeout 120 env ACCT_DATA_DIR="$(win "$PWD/$d")" ACCT_A11Y=1 "./$EXE" 2>&1 | grep -vE "$QUIET")
  local summary; summary=$(echo "$out" | grep -E "A11Y:")
  if echo "$out" | grep -q "verdict: PASS"; then
    echo "  [PASS] $label — $summary"
  else
    echo "  [FAIL] $label — $summary"
    echo "$out" | grep -E "UNNAMED" | sed 's/^/      /'
    fail=1
  fi
  rm -rf "$d"
}

echo
echo "== accessibility tree audit =="
run_a11y "empty-company" 0
run_a11y "with-data"     8

echo
[ "$fail" -eq 0 ] && echo "== ACCESSIBILITY: PASSED (every visible interactive control is named) ==" \
                  || echo "== ACCESSIBILITY: FAILED =="
exit "$fail"
