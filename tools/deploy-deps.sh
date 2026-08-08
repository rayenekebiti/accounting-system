#!/usr/bin/env bash
# deploy-deps.sh — bundle the MSYS2/ucrt64 third-party DLL closure that windeployqt
# leaves behind on MinGW builds.
#
# windeployqt copies Qt's own DLLs + plugins + QML modules, but NOT the MinGW
# ecosystem libraries they depend on (ICU, harfbuzz, freetype, PCRE2, glib, zstd,
# libpng, libjpeg, …). On the dev machine those resolve from ucrt64/bin on PATH, so
# the app "works"; on a clean machine without MSYS2 it dies at load with
# STATUS_DLL_NOT_FOUND (0xC0000135). This script walks the import tables of the exe
# AND every deployed plugin/QML-module DLL, recursively, and copies any missing
# ucrt64 dependency into the deploy dir.
#
# Implementation note: we use `objdump -p` (deterministic, reads the PE import
# table directly) — NOT `ldd`. MSYS2 `ldd` shells out to `ntldd`, which reads stdin
# and makes batch scans non-deterministic; it silently scanned only one file.
#
# Usage:  bash tools/deploy-deps.sh <deploy-dir>   (defaults to build/)
# Idempotent. Exit 0 = closure complete and verified.

set -u
DEPLOY="${1:-build}"
UCRT="/c/msys64/ucrt64/bin"
cd "$DEPLOY" || { echo "deploy dir not found: $DEPLOY"; exit 2; }
command -v objdump >/dev/null || { echo "objdump not on PATH (run under MSYS2/ucrt64)"; exit 2; }

imports()  { objdump -p "$1" 2>/dev/null | awk '/DLL Name:/{print $3}'; }
roots()    { find . -type f \( -iname '*.dll' -o -iname '*.exe' \); }

echo "== bundling ucrt64 closure into $DEPLOY =="

# Index every PE already in the tree by lowercased basename → path. A dependency is
# "satisfied" if its basename is in this index (top level for the exe's imports, or
# a qml/plugin subdir for Qt modules Qt resolves via its own import path). Hash
# lookups keep the recursive walk fast instead of a find() per import.
declare -A have
while IFS= read -r p; do have["$(basename "$p" | tr '[:upper:]' '[:lower:]')"]="$p"; done < <(roots)

declare -A seen
queue=()
while IFS= read -r p; do queue+=("$p"); done < <(roots)

added=0
i=0
while [ "$i" -lt "${#queue[@]}" ]; do
  f="${queue[$i]}"; i=$((i+1))
  while IFS= read -r name; do
    [ -n "$name" ] || continue
    key="${name,,}"
    [ -n "${seen[$key]:-}" ] && continue
    seen[$key]=1
    if [ -n "${have[$key]:-}" ]; then
      queue+=("${have[$key]}")                    # recurse into the deployed copy
    elif [ -f "$UCRT/$name" ]; then
      cp "$UCRT/$name" "./$name" && {
        echo "  + $name"; added=$((added+1)); have[$key]="./$name"; queue+=("./$name");
      }
    fi
    # else: a Windows system DLL (KERNEL32, api-ms-win-*, …) — never bundle.
  done < <(imports "$f")
done
echo "  added $added third-party DLL(s)"

# Verify: every ucrt64-resident import of every tree PE is now present.
missing=0
while IFS= read -r f; do
  while IFS= read -r name; do
    key="${name,,}"
    if [ -z "${have[$key]:-}" ] && [ -f "$UCRT/$name" ]; then
      echo "  MISSING $name  (needed by $f)"; missing=1
    fi
  done < <(imports "$f")
done < <(roots)

if [ "$missing" -ne 0 ]; then echo "== INCOMPLETE =="; exit 1; fi
echo "== closure complete: no unbundled ucrt64 dependency remains =="
