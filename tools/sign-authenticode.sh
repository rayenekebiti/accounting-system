#!/usr/bin/env bash
# sign-authenticode.sh — Authenticode-sign Windows PE files (the exe + the installer), or leave
# them unsigned in development. Deterministic + non-interactive; safe to call unconditionally.
#
# THREE-TIER MODEL (selected purely by environment — no prompts):
#   1. UNSIGNED (development) — no OCCOUNTANT_SIGN_CERT set:
#         prints a notice and exits 0, files unchanged. This is the default local/dev path.
#   2. TEST CERTIFICATE — OCCOUNTANT_SIGN_CERT points at a self-signed .pfx:
#         signs so the whole pipeline (and its regression test) can be exercised offline.
#   3. PRODUCTION CERTIFICATE — OCCOUNTANT_SIGN_CERT points at the real Authenticode .pfx
#         (ideally an EV cert / HSM); add OCCOUNTANT_SIGN_TS_URL for an RFC3161 countersignature.
#
# Environment:
#   OCCOUNTANT_SIGN_CERT   path to the signing .pfx/.p12   (absence ⇒ tier 1, unsigned)
#   OCCOUNTANT_SIGN_PASS   password for the .pfx           (optional)
#   OCCOUNTANT_SIGN_TS_URL RFC3161 timestamp URL           (optional; omitted ⇒ offline, no TS)
#   OCCOUNTANT_SIGNTOOL    explicit path to signtool.exe   (optional; else PATH, else osslsigncode)
#
# Usage:  bash tools/sign-authenticode.sh <file> [file...]
# Exit:   0 = signed OR intentionally-unsigned;  non-zero = a configured signing attempt FAILED.
set -u

CERT="${OCCOUNTANT_SIGN_CERT:-}"
PASS="${OCCOUNTANT_SIGN_PASS:-}"
TS="${OCCOUNTANT_SIGN_TS_URL:-}"

if [ "$#" -eq 0 ]; then echo "sign-authenticode: no files given"; exit 2; fi

if [ -z "$CERT" ]; then
  echo "sign-authenticode: no signing certificate configured (OCCOUNTANT_SIGN_CERT) — leaving files UNSIGNED (development tier)"
  for f in "$@"; do echo "  unsigned: $f"; done
  exit 0
fi
[ -f "$CERT" ] || { echo "sign-authenticode: certificate not found: $CERT"; exit 1; }

# Resolve a signer: prefer signtool (native Authenticode), fall back to osslsigncode (cross-platform).
SIGNTOOL="${OCCOUNTANT_SIGNTOOL:-}"
if [ -z "$SIGNTOOL" ] && command -v signtool >/dev/null 2>&1; then SIGNTOOL="$(command -v signtool)"; fi
OSSL=""
if [ -z "$SIGNTOOL" ] && command -v osslsigncode >/dev/null 2>&1; then OSSL="$(command -v osslsigncode)"; fi
if [ -z "$SIGNTOOL" ] && [ -z "$OSSL" ]; then
  echo "sign-authenticode: a certificate is configured but neither signtool nor osslsigncode was found"
  exit 1
fi

fail=0
for f in "$@"; do
  [ -f "$f" ] || { echo "  MISSING: $f"; fail=1; continue; }
  if [ -n "$SIGNTOOL" ]; then
    winf="$(cygpath -w "$f" 2>/dev/null || echo "$f")"
    winc="$(cygpath -w "$CERT" 2>/dev/null || echo "$CERT")"
    args=(sign /fd sha256 /f "$winc")
    [ -n "$PASS" ] && args+=(/p "$PASS")
    [ -n "$TS" ]   && args+=(/tr "$TS" /td sha256)
    if "$SIGNTOOL" "${args[@]}" "$winf"; then echo "  signed (signtool): $f"; else echo "  SIGN FAILED: $f"; fail=1; fi
  else
    args=(sign -pkcs12 "$CERT" -h sha256 -in "$f" -out "$f.signed")
    [ -n "$PASS" ] && args+=(-pass "$PASS")
    [ -n "$TS" ]   && args+=(-ts "$TS")
    if "$OSSL" "${args[@]}" && mv -f "$f.signed" "$f"; then echo "  signed (osslsigncode): $f"; else echo "  SIGN FAILED: $f"; rm -f "$f.signed"; fail=1; fi
  fi
done
exit "$fail"
