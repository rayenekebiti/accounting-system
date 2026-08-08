#!/usr/bin/env bash
# security-gate.sh — deterministic, offline security checks for the release pipeline.
#
# The crypto/licensing/update-integrity surface is covered by ACCT_C2TEST (tampered license →
# Invalid, forged update signature → not staged, corrupt cache ignored). THIS gate covers the
# release-specific security concern the C2 tests don't: that the SUPPORT/CRASH artifacts we ship
# or ask users to send can never carry accounting data out of the books.
#
# It seeds a real dataset, generates a support bundle + a crash report, and proves:
#   • no data file (*.dat / *.manifest / audit / journal) is present in either artifact
#   • no data file's bytes are smuggled in verbatim (sha256 of every data file ∉ bundle entries)
#   • the redaction primitive masks currency values (checked in-process by ACCT_C2TEST)
#
# Usage:  bash tools/security-gate.sh
set -u
cd "$(dirname "$0")/.." || exit 2
BUILD=build
EXE="$BUILD/AccountingQuick.exe"
win() { cygpath -m "$1"; }
QUIET='catalogs.json|Translations will not be available'
fail=0

echo "== building =="
cmake --build "$BUILD" --target AccountingQuick 2>&1 | grep -iE "error:|error C" && { echo "BUILD FAILED"; exit 1; }
[ -f "$EXE" ] || { echo "missing $EXE"; exit 1; }

D="$BUILD/.security_gate"; rm -rf "$D"; mkdir -p "$D"; WD="$(win "$PWD/$D")"

echo "== seed a real dataset (books with values on disk) =="
env ACCT_DATA_DIR="$WD" ACCT_BENCH_SEED=20 "./$EXE" >/dev/null 2>&1
datfiles=$(find "$D" -maxdepth 1 -name '*.dat' | wc -l)
echo "  data files on disk: $datfiles"
[ "$datfiles" -ge 1 ] || { echo "  [FAIL] seeding produced no .dat files — cannot test exfiltration"; exit 1; }

echo "== generate a support bundle from the seeded books =="
env ACCT_DATA_DIR="$WD" ACCT_SUPPORT_BUNDLE="$(win "$PWD/$D/SupportBundle.zip")" "./$EXE" >/dev/null 2>&1
[ -f "$D/SupportBundle.zip" ] || { echo "  [FAIL] no support bundle produced"; exit 1; }

# Also produce a crash report zip via the C2 self-test path (writes CrashReport*.zip under the scratch).
env ACCT_C2TEST="$(win "$PWD/$D/c2")" "./$EXE" >/dev/null 2>&1

# Collect every artifact zip we ship or ask users to send.
mapfile -t ARTIFACTS < <(find "$D" -name '*.zip')

echo "== check: no data-file NAME appears inside any artifact =="
banned='\.dat$|\.manifest$|\.journal$|(^|/)audit\.log$|(^|/)events\.log$|(^|/)compat\.manifest$'
for z in "${ARTIFACTS[@]}"; do
  names=$(unzip -Z1 "$z" 2>/dev/null)
  hit=$(echo "$names" | grep -iE "$banned" || true)
  if [ -n "$hit" ]; then echo "  [FAIL] $(basename "$z") contains data file(s):"; echo "$hit" | sed 's/^/      /'; fail=1;
  else echo "  [ok]   $(basename "$z"): no data-file names"; fi
done

echo "== check: no data file's BYTES are smuggled in verbatim (sha256) =="
# Hash every on-disk data file, then hash every entry inside every artifact; intersection must be empty.
datahashes="$D/.datahashes"; : > "$datahashes"
while IFS= read -r f; do sha256sum "$f" | awk '{print $1}' >> "$datahashes"; done < <(find "$D" -maxdepth 1 \( -name '*.dat' -o -name '*.manifest' \))
extract="$D/.extract"; rm -rf "$extract"; mkdir -p "$extract"
leak=0
for z in "${ARTIFACTS[@]}"; do
  sub="$extract/$(basename "$z" .zip)"; mkdir -p "$sub"
  unzip -o -q "$z" -d "$sub" 2>/dev/null
  while IFS= read -r ef; do
    h=$(sha256sum "$ef" | awk '{print $1}')
    if grep -qx "$h" "$datahashes"; then echo "  [FAIL] $(basename "$z") entry $ef == a data file (verbatim)"; leak=1; fi
  done < <(find "$sub" -type f)
done
[ "$leak" -eq 0 ] && echo "  [ok]   no data-file bytes found in any artifact" || fail=1

echo
[ "$fail" -eq 0 ] && echo "== SECURITY GATE: PASSED ==" || echo "== SECURITY GATE: FAILED =="
exit "$fail"
