#!/usr/bin/env bash
# release-test.sh — regression-test tools/release.sh itself: its FAILURE PATHS (fail-fast, nothing
# packaged) and the integrity of a SUCCESSFUL package (checksums + update-manifest signature).
# Deterministic + offline. Does not run the heavy gates — it injects/asserts around them.
#
# Usage:  bash tools/release-test.sh
set -u
cd "$(dirname "$0")/.." || exit 2
EXE=build/AccountingQuick.exe
win() { cygpath -m "$1"; }
[ -f "$EXE" ] || { echo "build AccountingQuick first"; exit 1; }
fail=0
OUTROOT=build/.reltest

# ── 1. Invalid channel is rejected before anything is built or written ─────────
echo "== failure path: invalid channel =="
rm -rf "$OUTROOT"
bash tools/release.sh --channel nonsense --out "$OUTROOT" >/dev/null 2>&1
rc=$?
if [ "$rc" -ne 0 ] && [ ! -d "$OUTROOT" ]; then echo "  [PASS] invalid channel → exit $rc, no artifacts"; else echo "  [FAIL] rc=$rc outdir=$([ -d "$OUTROOT" ] && echo present || echo absent)"; fail=1; fi

# ── 2. A failing gate aborts the release fail-fast; NOTHING is packaged ─────────
echo "== failure path: a gate fails → fail-fast, no package =="
rm -rf "$OUTROOT"
out=$(OCCOUNTANT_RELEASE_SELFTEST_FAIL=security bash tools/release.sh --no-build --only security \
        --version 9.9.9 --channel beta --out "$OUTROOT" 2>&1)
rc=$?
if [ "$rc" -ne 0 ] && echo "$out" | grep -q "gate 'security' failed" && [ ! -d "$OUTROOT/9.9.9-beta" ]; then
  echo "  [PASS] gate failure → exit $rc, aborted before packaging"
else
  echo "  [FAIL] rc=$rc"; echo "$out" | tail -3 | sed 's/^/      /'; fail=1
fi

# ── 3. Successful package: artifacts present, checksums + update signature valid ─
echo "== success path: package integrity (checksums + update-manifest signature) =="
rm -rf "$OUTROOT"
if bash tools/release.sh --package-only --version 9.9.8 --channel rc --out "$OUTROOT" >/dev/null 2>&1; then
  O="$OUTROOT/9.9.8-rc"
  # 3a) all artifacts present
  miss=0
  for f in manifest.json release-manifest.json SHA256SUMS BuildInfo.json RELEASE_NOTES.md Occountant-9.9.8-portable.zip; do
    [ -f "$O/$f" ] || { echo "  [FAIL] missing artifact: $f"; miss=1; }
  done
  [ "$miss" -eq 0 ] && echo "  [ok] all expected artifacts present"
  # 3b) checksums verify
  if ( cd "$O" && sha256sum -c SHA256SUMS >/dev/null 2>&1 ); then echo "  [ok] SHA256SUMS verify"; else echo "  [FAIL] SHA256SUMS mismatch"; fail=1; fi
  # 3c) the update manifest's signature verifies against the payload it names (round-trip via ACCT_SIGN)
  payload=$(grep -o '"payload": *"[^"]*"' "$O/manifest.json" | cut -d'"' -f4)
  claimed=$(grep -o '"sig": *"[^"]*"'     "$O/manifest.json" | cut -d'"' -f4)
  actual=$(env ACCT_DATA_DIR="$(win "$PWD/build/.reltest_sig")" ACCT_SIGN="$(win "$PWD/$O/$payload")" "./$EXE" 2>/dev/null | tr -d '\r\n')
  if [ -n "$claimed" ] && [ "$claimed" = "$actual" ]; then echo "  [ok] update-manifest signature matches payload"; else echo "  [FAIL] sig mismatch (claimed=$claimed actual=$actual)"; fail=1; fi
  # 3d) manifest metadata matches the requested build
  if grep -q '"channel": "rc"' "$O/manifest.json" && grep -q '"versionCode": 9009008' "$O/manifest.json"; then
    echo "  [ok] manifest records channel + version code"
  else echo "  [FAIL] manifest metadata wrong"; fail=1; fi
  [ "$miss" -eq 0 ] || fail=1
else
  echo "  [FAIL] package-only run failed"; fail=1
fi
rm -rf "$OUTROOT" build/.reltest_sig

echo
[ "$fail" -eq 0 ] && echo "== RELEASE SCRIPT: ALL PASSED ==" || echo "== RELEASE SCRIPT: FAILED =="
exit "$fail"
