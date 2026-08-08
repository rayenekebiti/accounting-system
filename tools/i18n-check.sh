#!/usr/bin/env bash
# i18n-check.sh — translation regression guardrails for CI.
# Run from the repo root (or anywhere; it cd's to its own repo root).
# Exits non-zero if any check fails, with a human-readable report.
#
# Checks:
#   1. Hardcoded user-facing QML literals not wrapped in qsTr (new untranslated strings).
#   2. Coverage: every <source> in app_en.ts exists in app_fr.ts AND app_ar.ts.
#   3. No empty / "unfinished" translations remain in fr/ar.
#   4. Arabic plural messages have all 6 <numerusform> entries; French have 2.
#   5. Catalog freshness: qsTr count in QML vs message count in en.ts (heuristic).

set -u
cd "$(dirname "$0")/.." || exit 2
QML=quick/qml
I18N=quick/i18n
fail=0
note() { echo "  - $1"; }
section() { echo ""; echo "== $1 =="; }

# ── 1. Hardcoded user-facing literals (not qsTr) ──────────────────────────────
# Heuristic: text/title/label/placeholder/message/etc. assigned a quoted literal,
# excluding glyphs, format hints, pure numbers/punctuation, and the dev Gallery.
section "1. Hardcoded user-facing literals (should be qsTr)"
hard=$(grep -rnE '(text|title|subtitle|label|placeholder|searchPlaceholder|message|confirmText|cancelText|actionText|description):[[:space:]]*"[^"]' "$QML"/*.qml \
  | grep -v Gallery.qml \
  | grep -v 'qsTr' \
  | grep -vE ':[0-9]+:[[:space:]]*//' \
  | grep -vE ':[[:space:]]*"(✕|🌐|🧾|👤|⌕|＋|⚠|✓|—|\$|%[0-9]|YYYY-MM-DD|Occountant|[0-9.,:\$ /-]*)"' )
if [ -n "$hard" ]; then
  echo "$hard" | while IFS= read -r l; do note "$l"; done
  echo "FAIL: hardcoded literal(s) above must use qsTr()."
  fail=1
else
  echo "OK: no hardcoded user-facing literals."
fi

# ── 2. Coverage: every en source present in fr + ar ───────────────────────────
section "2. Catalog coverage (en sources present in fr + ar)"
grep -oE '<source>[^<]+</source>' "$I18N/app_en.ts" | sort -u > /tmp/_en.$$
for lang in fr ar; do
  grep -oE '<source>[^<]+</source>' "$I18N/app_$lang.ts" | sort -u > /tmp/_$lang.$$
  missing=$(comm -23 /tmp/_en.$$ /tmp/_$lang.$$)
  if [ -n "$missing" ]; then
    echo "FAIL: app_$lang.ts is missing sources:"
    echo "$missing" | while IFS= read -r m; do note "$m"; done
    fail=1
  else
    echo "OK: app_$lang.ts covers all en sources."
  fi
  rm -f /tmp/_$lang.$$
done
rm -f /tmp/_en.$$

# ── 3. No empty / unfinished translations ─────────────────────────────────────
section "3. No empty / unfinished translations (fr, ar)"
for lang in fr ar; do
  e=$(grep -cE '<translation[^>]*></translation>|<numerusform></numerusform>' "$I18N/app_$lang.ts")
  u=$(grep -c 'type="unfinished"' "$I18N/app_$lang.ts")
  if [ "$e" -ne 0 ] || [ "$u" -ne 0 ]; then
    echo "FAIL: app_$lang.ts has $e empty + $u unfinished translation(s)."
    fail=1
  else
    echo "OK: app_$lang.ts fully translated."
  fi
done

# ── 4. Plural-form completeness ───────────────────────────────────────────────
section "4. Plural forms (ar=6, fr=2 per numerus message)"
check_plurals() {
  # NB: separate `local` statements — referencing $lang in the same `local` that
  # assigns it would expand the OLD value (bash evaluates all RHS first).
  local lang=$1
  local want=$2
  local f="$I18N/app_$lang.ts"
  # Accumulate each numerus <message> block and count <numerusform> OCCURRENCES
  # (forms may all be on one line, so count occurrences, not matching lines).
  awk -v want="$want" '
    /<message numerus="yes">/ { inmsg=1; buf="" }
    inmsg                      { buf = buf $0 }
    inmsg && /<\/message>/     { n = gsub(/<numerusform>/, "x", buf);
                                 if (n != want) { print "    bad: "n" forms (want "want")"; bad++ }
                                 inmsg=0 }
    END { if (bad) exit 1 }
  ' "$f"
}
for pair in "ar 6" "fr 2"; do
  set -- $pair
  if check_plurals "$1" "$2"; then echo "OK: app_$1.ts plural forms = $2."
  else echo "FAIL: app_$1.ts has a numerus message with wrong form count."; fail=1; fi
done

# ── 5. Catalog freshness heuristic ────────────────────────────────────────────
section "5. Catalog freshness (qsTr in QML vs en.ts messages)"
qtr=$(grep -rho 'qsTr(' "$QML"/*.qml | grep -v Gallery | wc -l | tr -d ' ')
msgs=$(grep -c '<message' "$I18N/app_en.ts" | tr -d ' ')
echo "  qsTr() calls (excl. Gallery): $qtr   |   en.ts messages: $msgs"
if [ "$msgs" -lt "$((qtr / 2))" ]; then
  echo "FAIL: en.ts looks stale — far fewer messages than qsTr calls. Run tools/i18n-extract.ps1."
  fail=1
else
  echo "OK: catalog message count is consistent with qsTr usage."
fi

# ── 6. Forbidden concatenation of translated text ─────────────────────────────
# qsTr(...) joined to a literal containing letters (a word) — or the reverse —
# means a phrase was built by concatenation instead of qsTr("... %1").arg().
# Joining qsTr to a neutral separator (" · ", " - ") is allowed.
section "6. No concatenation of translated strings"
concat=$(grep -rnE 'qsTr\([^)]*\)[[:space:]]*\+[[:space:]]*"[^"]*[A-Za-z]|"[^"]*[A-Za-z][^"]*"[[:space:]]*\+[[:space:]]*qsTr' "$QML"/*.qml | grep -v Gallery.qml)
if [ -n "$concat" ]; then
  echo "$concat" | while IFS= read -r l; do note "$l"; done
  echo "FAIL: build the phrase with qsTr(\"... %1\").arg(x) instead of concatenation."
  fail=1
else
  echo "OK: no translated-string concatenation."
fi

# ── 7. Directional-alignment misuse in mirrored layouts ───────────────────────
# (a) Choosing alignment from Theme.rtl / layoutDirection double-flips under
#     LayoutMirroring (the classic bug). Always forbidden.
# (b) A bare Text.AlignLeft/AlignRight is allowed only with a justifying comment
#     on the same line (LayoutMirroring already flips logical alignment for you).
section "7. Logical alignment only (no RTL alignment hacks)"
align_cond=$(grep -rnE '(Theme\.rtl|layoutDirection)[^?]*\?[^:]*(AlignRight|AlignLeft)' "$QML"/*.qml | grep -v Gallery.qml)
align_bare=$(grep -rnE 'horizontalAlignment:[[:space:]]*Text\.(AlignLeft|AlignRight)' "$QML"/*.qml \
  | grep -v Gallery.qml | grep -vE '//')
if [ -n "$align_cond" ] || [ -n "$align_bare" ]; then
  [ -n "$align_cond" ] && { echo "$align_cond" | while IFS= read -r l; do note "rtl-conditional: $l"; done; }
  [ -n "$align_bare" ] && { echo "$align_bare" | while IFS= read -r l; do note "unjustified: $l"; done; }
  echo "FAIL: use logical Text.AlignLeft (mirroring flips it); justify any explicit alignment with a comment."
  fail=1
else
  echo "OK: alignment is logical / justified."
fi

# ── 8. Translated helper functions must depend on i18n.language (live retranslate) ──
# A QML function that returns qsTr(...) but is called from a BINDING (e.g. `text: statusLabel(x)`)
# does NOT re-run on a live language switch — retranslate() only re-evaluates bindings whose text
# contains qsTr directly. The fix is a bare `i18n.language` reference inside the function so the
# binding depends on the language. Signal handlers (`function onXxx`) are imperative (one-shot) and
# excluded. Guards the whole codebase against the "labels stuck in the previous language" bug (C12-R).
section "8. Translated helpers depend on i18n.language (live retranslate)"
retrans=""
for f in "$QML"/*.qml; do
  case "$f" in *Gallery.qml) continue;; esac
  bad=$(awk '
    /function [A-Za-z_]+\(/ { inf=1; fname=$0; buf=""; depth=0 }
    inf { buf = buf "\n" $0 }
    inf { n=gsub(/{/,"{"); depth+=n; m=gsub(/}/,"}"); depth-=m }
    inf && depth<=0 && buf ~ /}/ {
       if (fname !~ /function[ \t]+on[A-Z]/ && buf ~ /qsTr\(/ && buf !~ /i18n\.language/) {
         sub(/^[ \t]*/,"",fname); print fname
       }
       inf=0
    }
  ' "$f")
  [ -n "$bad" ] && retrans="$retrans
  $f — $bad"
done
if [ -n "$retrans" ]; then
  echo "$retrans" | while IFS= read -r l; do [ -n "$l" ] && note "$l"; done
  echo "FAIL: add a bare 'i18n.language' reference inside the function so its binding retranslates."
  fail=1
else
  echo "OK: translated helper functions depend on i18n.language."
fi

echo ""
if [ "$fail" -ne 0 ]; then echo "i18n-check: FAILED"; exit 1; fi
echo "i18n-check: PASSED"
