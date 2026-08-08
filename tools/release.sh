#!/usr/bin/env bash
# release.sh — one command from source to a shippable Occountant release. Deterministic, offline.
#
#   Developer → Build → Sign → Package → (Publish) → Install → Activate → Update → Backup → Recover
#
# Pipeline:
#   1. BUILD    stamped Release build (channel + build-id embedded via BuildInfo)
#   2. GATES    ptest · itest · fuzz · perf · c2test · compat-verify · security · i18n · shots · upgrade
#               — run fail-fast; the FIRST failure aborts the release (nothing is packaged).
#   3. STAGE    clean, clean-machine runtime tree (tools/stage-runtime.sh) with BuildInfo.json
#   4. PACKAGE  installer (if Inno present) + portable ZIP + release notes + update manifest
#               + Authenticode signature (if a cert is configured) + SHA256SUMS + release manifest
#
# Graceful degradation (matches "Sign if key available"): the portable ZIP + manifests + checksums
# are ALWAYS produced with only the repo toolchain; the Inno installer is built only if `iscc` is
# found, and Authenticode signing runs only if OCCOUNTANT_SIGN_CERT is set. Missing OPTIONAL tools
# are a warning, not a failure; a failing GATE is always fatal.
#
# Usage:
#   bash tools/release.sh [--version X.Y.Z] [--channel stable|rc|beta|development] [--build-id ID]
#                         [--skip-gates a,b,c] [--no-build] [--gates-only] [--package-only]
#                         [--smoke] [--out DIR]
#   --smoke: after packaging, run tools/smoke-install.ps1 (install → launch → empty company →
#            activate license → uninstall-preserves-data) against the produced installer.
# Output: dist/release/<version>-<channel>/
set -u
cd "$(dirname "$0")/.." || exit 2

VERSION="1.0.0"; CHANNEL="stable"; BUILD_ID=""; SKIP=""; ONLY=""
DO_BUILD=1; DO_GATES=1; DO_PACKAGE=1; DO_SMOKE=0; OUTROOT="dist/release"

while [ $# -gt 0 ]; do
  case "$1" in
    --version)      VERSION="$2"; shift 2;;
    --channel)      CHANNEL="$2"; shift 2;;
    --build-id)     BUILD_ID="$2"; shift 2;;
    --skip-gates)   SKIP="$2"; shift 2;;
    --only)         ONLY="$2"; shift 2;;      # run only this one gate (used by release-test.sh)
    --no-build)     DO_BUILD=0; shift;;
    --gates-only)   DO_PACKAGE=0; shift;;
    --package-only) DO_BUILD=0; DO_GATES=0; shift;;
    --smoke)        DO_SMOKE=1; shift;;        # after packaging, run the install→activate smoke test
    --out)          OUTROOT="$2"; shift 2;;
    -h|--help)      grep '^#' "$0" | sed 's/^#\{1,\} \{0,1\}//'; exit 0;;
    *) echo "release: unknown arg: $1"; exit 2;;
  esac
done

case "$CHANNEL" in stable|rc|beta|development) ;; *) echo "release: invalid channel '$CHANNEL'"; exit 2;; esac

BUILD=build
EXE="$BUILD/AccountingQuick.exe"
STAGE="dist/Occountant"
GITSHORT="$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"
[ -z "$BUILD_ID" ] && BUILD_ID="$(date -u +%Y%m%d).$GITSHORT"
IFS=. read -r VMAJ VMIN VPAT <<< "$VERSION"
VCODE=$(( ${VMAJ:-0}*1000000 + ${VMIN:-0}*1000 + ${VPAT:-0} ))
OUT="$OUTROOT/$VERSION-$CHANNEL"
OUTABS="$PWD/$OUT"

win()  { cygpath -m "$1"; }
winw() { cygpath -w "$1"; }
step() { echo; echo "════════ $* ════════"; }
die()  { echo; echo "release: FAILED — $*" >&2; exit 1; }
skipped() { echo "$SKIP" | tr ',' '\n' | grep -qx "$1"; }
gate() {
  local n="$1"; shift
  [ -n "$ONLY" ] && [ "$ONLY" != "$n" ] && return 0     # --only <gate>: run just that one
  if skipped "$n"; then echo "  ~ skip: $n"; return 0; fi
  # Deterministic failure-injection seam for release-test.sh (proves fail-fast aborts the release).
  if [ "${OCCOUNTANT_RELEASE_SELFTEST_FAIL:-}" = "$n" ]; then echo "  ▶ $n (forced fail — self-test)"; die "gate '$n' failed"; fi
  echo "  ▶ $n"; if "$@"; then echo "  ✓ $n"; else die "gate '$n' failed"; fi
}

run_c2test() { local d="$BUILD/.rel_c2"; rm -rf "$d"; mkdir -p "$d"
  env ACCT_C2TEST="$(win "$PWD/$d")" "./$EXE" >/dev/null 2>&1; }
run_compat_verify() { local d="$BUILD/.rel_compat"; rm -rf "$d"; mkdir -p "$d"
  env ACCT_DATA_DIR="$(win "$PWD/$d")" ACCT_BENCH_SEED=10 "./$EXE" >/dev/null 2>&1
  env ACCT_DATA_DIR="$(win "$PWD/$d")" ACCT_COMPAT_VERIFY=1 "./$EXE" >/dev/null 2>&1; }

sha256() { sha256sum "$1" | awk '{print $1}'; }
make_zip() { # <parentDir> <topName> <absOutZip>
  local parent="$1" top="$2" out="$3"; rm -f "$out"
  if command -v zip >/dev/null 2>&1; then ( cd "$parent" && zip -rqX "$out" "$top" )
  else powershell -NoProfile -Command "Compress-Archive -Path '$(winw "$parent/$top")' -DestinationPath '$(winw "$out")' -Force"; fi
  [ -f "$out" ]; }

echo "Occountant release  version=$VERSION  channel=$CHANNEL  build=$BUILD_ID  code=$VCODE  commit=$GITSHORT"

# ── 1) BUILD ─────────────────────────────────────────────────────────────────
if [ "$DO_BUILD" -eq 1 ]; then
  step "Build (Release · channel=$CHANNEL · build-id=$BUILD_ID)"
  # ACCT_DEV_SIGNING=OFF: the shipped artifact embeds only the update PUBLIC key — it can VERIFY an
  # update but not SIGN one (no Ed25519 secret compiled in). The vendor signs releases from a
  # separate dev/signing build. See docs/update-signing.md.
  cmake -S . -B "$BUILD" -G Ninja \
        -DACCT_RELEASE_CHANNEL="$CHANNEL" -DACCT_RELEASE_BUILD_ID="$BUILD_ID" \
        -DACCT_DEV_SIGNING=OFF >/dev/null || die "cmake configure failed"
  cmake --build "$BUILD" --target AccountingQuick 2>&1 | grep -iE "error:|error C" && die "compile failed"
fi
[ -f "$EXE" ] || die "executable not built: $EXE (run without --package-only, or build first)"

# ── 2) GATES (fail on first error) ───────────────────────────────────────────
if [ "$DO_GATES" -eq 1 ]; then
  step "Validation gates (fail-fast)"
  gate ptest    bash tools/ptest.sh
  gate itest    bash tools/itest.sh
  gate fuzz     bash tools/fuzz.sh
  gate perf     bash tools/perf.sh
  gate c2test   run_c2test
  gate compat   run_compat_verify
  gate security bash tools/security-gate.sh
  gate i18n     bash tools/i18n-check.sh
  gate shots    bash tools/shots.sh
  gate upgrade  bash tools/upgrade-test.sh
fi

[ "$DO_PACKAGE" -eq 1 ] || { step "Done (gates only)"; exit 0; }

# ── 3) STAGE ─────────────────────────────────────────────────────────────────
step "Stage clean runtime tree"
bash tools/stage-runtime.sh "$BUILD" "$STAGE" || die "staging failed"

# ── 4) PACKAGE ───────────────────────────────────────────────────────────────
step "Assemble release artifacts → $OUT"
rm -rf "$OUT"; mkdir -p "$OUT"

# 4a) Installer (Inno) — only if iscc is available.
ISCC="$(command -v iscc 2>/dev/null || true)"
if [ -z "$ISCC" ]; then for p in "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" "/c/Program Files/Inno Setup 6/ISCC.exe"; do [ -x "$p" ] && ISCC="$p"; done; fi
INSTALLER=""
if [ -n "$ISCC" ]; then
  echo "  building installer (Inno Setup)…"
  "$ISCC" /Q "/DAppVersion=$VERSION" "/DVersionCode=$VCODE" \
          "/DStageDir=$(winw "$PWD/$STAGE")" "/DOutputDir=$(winw "$OUTABS")" \
          "$(winw "$PWD/installer/Occountant.iss")" || die "iscc failed"
  INSTALLER="$OUT/Occountant-$VERSION-Setup.exe"
  [ -f "$INSTALLER" ] || die "installer not produced: $INSTALLER"
  echo "  OK $INSTALLER"
else
  echo "  ~ Inno Setup (iscc) not found — installer SKIPPED (portable ZIP still produced)"
fi

# 4b) Portable ZIP (always) — the staged tree + a portable marker so it runs from anywhere.
echo "  building portable ZIP…"
PDIR="$BUILD/.portable"; rm -rf "$PDIR"; mkdir -p "$PDIR/Occountant"
cp -r "$STAGE/." "$PDIR/Occountant/"
: > "$PDIR/Occountant/Occountant.portable"          # marker → app stores data next to the exe
PZIP="$OUT/Occountant-$VERSION-portable.zip"
make_zip "$PWD/$PDIR" "Occountant" "$OUTABS/Occountant-$VERSION-portable.zip" || die "portable ZIP failed"
echo "  OK $PZIP"

# 4c) BuildInfo + release notes.
cp "$STAGE/BuildInfo.json" "$OUT/BuildInfo.json"
NOTES="$OUT/RELEASE_NOTES.md"
if [ -f RELEASE_NOTES.md ]; then cp RELEASE_NOTES.md "$NOTES"; else
  cat > "$NOTES" <<EOF
# Occountant $VERSION ($CHANNEL)

- Build: $BUILD_ID
- Commit: $GITSHORT
- Channel: $CHANNEL
- Publisher: RIO&JHK Technologies Co.

This is a deterministic, offline build produced by \`tools/release.sh\`. The accounting engine is
byte-identical and replay-equivalent to the previous release; changes are confined to the release/
distribution layer above it. User data in %LOCALAPPDATA%\\Occountant is preserved across upgrades.
EOF
fi

# 4d) Authenticode-sign the installer FIRST (so its bytes are final before hashing/signing below).
if [ -n "$INSTALLER" ]; then
  echo "  Authenticode signing…"
  bash tools/sign-authenticode.sh "$INSTALLER" || die "Authenticode signing failed"
  # Publish the SIGNED installer under the stable, unversioned name too, so download links and the
  # smoke test can hardcode "Occountant-Setup.exe" while archives keep the versioned filename.
  cp "$INSTALLER" "$OUT/Occountant-Setup.exe"
  echo "  OK $OUT/Occountant-Setup.exe (stable alias of the signed installer)"
fi

# 4e) Update manifest (the exact format UpdateManager.check() consumes) over the FINAL payload.
PAYLOAD="${INSTALLER:-$PZIP}"
SIGDIR="$BUILD/.rel_sign"; rm -rf "$SIGDIR"; mkdir -p "$SIGDIR"
USIG="$(env ACCT_DATA_DIR="$(win "$PWD/$SIGDIR")" ACCT_SIGN="$(win "$PWD/$PAYLOAD")" "./$EXE" 2>/dev/null | tr -d '\r\n')"
[ -n "$USIG" ] || die "could not compute update payload signature"
PSIZE=$(stat -c%s "$PAYLOAD")
cat > "$OUT/manifest.json" <<EOF
{
  "version": "$VERSION",
  "versionCode": $VCODE,
  "channel": "$CHANNEL",
  "payload": "$(basename "$PAYLOAD")",
  "size": $PSIZE,
  "sig": "$USIG",
  "notes": "Occountant $VERSION ($CHANNEL)"
}
EOF

# 4f) Release manifest (human/index) + SHA256SUMS over every artifact.
REL="$OUT/release-manifest.json"
{
  echo "{"
  echo "  \"product\": \"Occountant\","
  echo "  \"vendor\": \"RIO&JHK Technologies Co.\","
  echo "  \"version\": \"$VERSION\","
  echo "  \"versionCode\": $VCODE,"
  echo "  \"channel\": \"$CHANNEL\","
  echo "  \"buildId\": \"$BUILD_ID\","
  echo "  \"commit\": \"$GITSHORT\","
  echo "  \"artifacts\": ["
  first=1
  while IFS= read -r f; do
    b="$(basename "$f")"
    [ "$b" = "release-manifest.json" ] && continue
    [ "$b" = "SHA256SUMS" ] && continue
    [ $first -eq 1 ] && first=0 || echo ","
    printf '    { "name": "%s", "bytes": %s, "sha256": "%s" }' "$b" "$(stat -c%s "$f")" "$(sha256 "$f")"
  done < <(find "$OUT" -maxdepth 1 -type f | sort)
  echo
  echo "  ]"
  echo "}"
} > "$REL"

( cd "$OUT" && find . -maxdepth 1 -type f ! -name SHA256SUMS -printf '%P\n' | sort | while read -r n; do echo "$(sha256 "$n")  $n"; done > SHA256SUMS )

# ── Summary ──────────────────────────────────────────────────────────────────
step "Release complete → $OUT"
( cd "$OUT" && for f in *; do [ -f "$f" ] && printf '  %10s  %s\n' "$(stat -c%s "$f")" "$f"; done )
[ -z "$INSTALLER" ] && echo "  (installer skipped: install Inno Setup 6 and re-run to produce Occountant-$VERSION-Setup.exe)"

# ── 5) SMOKE (optional) — install → launch → empty company → activate license → uninstall ──────────
if [ "$DO_SMOKE" -eq 1 ]; then
  step "Post-package smoke test (install → activate → uninstall)"
  PS="$(command -v powershell 2>/dev/null || command -v pwsh 2>/dev/null || true)"
  if [ -z "$PS" ]; then
    echo "  ~ powershell not found — smoke test SKIPPED"
  else
    SMOKE_ARGS=(-ExecutionPolicy Bypass -File "$(winw "$PWD/tools/smoke-install.ps1")")
    [ -n "$INSTALLER" ] && SMOKE_ARGS+=(-Setup "$(winw "$OUTABS/Occountant-Setup.exe")")
    "$PS" -NoProfile "${SMOKE_ARGS[@]}" || die "smoke test failed"
  fi
fi

echo
echo "release: OK"
