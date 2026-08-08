#!/usr/bin/env bash
# acceptance.sh — run every realistic small-business persona through the FULL lifecycle and prove
# each finishes successfully. Each persona runs in its own fresh, isolated data dir (a brand-new
# empty company), driving the real editor ViewModels + commercial managers:
#   company creation → customers → suppliers → expenses → invoices (VAT) → payments + allocation →
#   VAT summary → reports → backup → restore verification → license → update staging → verifyAll.
#
# Deterministic + offline. Changes no accounting semantics.
# Usage:  bash tools/acceptance.sh
set -u
cd "$(dirname "$0")/.." || exit 2
BUILD=build
EXE="$BUILD/AccountingQuick.exe"
win() { cygpath -m "$1"; }
QUIET='catalogs.json|Translations will not be available|acct\.startup'
PERSONAS="cafe retail freelancer consultant repair clinic"
fail=0

echo "== building =="
cmake --build "$BUILD" --target AccountingQuick 2>&1 | grep -iE "error:|error C" && { echo "BUILD FAILED"; exit 1; }
[ -f "$EXE" ] || { echo "missing $EXE"; exit 1; }

echo
echo "== running acceptance personas (each a fresh company) =="
for p in $PERSONAS; do
  D="$BUILD/.accept_$p"; rm -rf "$D"; mkdir -p "$D"
  out=$(timeout 180 env ACCT_DATA_DIR="$(win "$PWD/$D")" ACCT_ACCEPT="$p" "./$EXE" 2>&1 | grep -vE "$QUIET")
  rc=${PIPESTATUS[0]}
  summary=$(echo "$out" | grep -E "^== $p:")
  if [ "$rc" = "0" ] && echo "$summary" | grep -q " 0 failed "; then
    echo "  [PASS] ${summary#== }"
  else
    echo "  [FAIL] $p (rc=$rc)"
    echo "$out" | grep -E "FAIL" | sed 's/^/        /'
    fail=1
  fi
  rm -rf "$D"
done

echo
[ "$fail" -eq 0 ] && echo "== ACCEPTANCE: ALL PERSONAS PASSED ==" || echo "== ACCEPTANCE: FAILED =="
exit "$fail"
