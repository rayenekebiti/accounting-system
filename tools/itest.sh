#!/usr/bin/env bash
# itest.sh — run the QML interaction test suite (deterministic, isolated, CI-gating).
# Builds the app, seeds an isolated dataset, drives the REAL QML interaction paths
# (property assignment, model invokables, control signal handlers), asserts VM
# state + persistence, and FAILS on any QML runtime error. Exit 0 = all passed.
#
# Usage:  bash tools/itest.sh

set -u
cd "$(dirname "$0")/.." || exit 2
BUILD=build
EXE="$BUILD/AccountingQuick.exe"
DATA="$BUILD/.itestdata"            # isolated, git-ignored
win() { cygpath -m "$1"; }

echo "== building =="
cmake --build "$BUILD" --target AccountingQuick 2>&1 | grep -iE "error:|error C" && { echo "BUILD FAILED"; exit 1; }
[ -f "$EXE" ] || { echo "missing $EXE"; exit 1; }

echo "== seeding isolated dataset =="
rm -rf "$DATA"; mkdir -p "$DATA"
env ACCT_DATA_DIR="$(win "$PWD/$DATA")" ACCT_SEED=1 "./$EXE" 2>/dev/null

echo "== running interaction tests =="
env ACCT_DATA_DIR="$(win "$PWD/$DATA")" ACCT_ITEST=1 "./$EXE" 2>&1 \
  | grep -vE "catalogs.json|Translations will not be available"
status=${PIPESTATUS[0]}

echo "== exit: $status (0 = all passed) =="
exit "$status"
