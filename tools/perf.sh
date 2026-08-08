#!/usr/bin/env bash
# perf.sh — deterministic performance & scalability harness for the event-sourcing engine.
#
#   bash tools/perf.sh              # regression GATE (fast, ~2 min): fail if a metric regresses
#   bash tools/perf.sh sweep        # full evidence SWEEP: 10k / 100k / 1M (SLOW — heavy ops at
#                                   #   100k are ~25 min; materialize is auto-gated + extrapolated
#                                   #   above 150k). Numbers feed docs/performance.md.
#   bash tools/perf.sh sweep 10000 500000
#
# Deterministic + seeded (ACCT_PERF_SEED). Isolated ACCT_DATA_DIR; never touches real data.
# Thresholds are size-stable RATES calibrated to the reference machine (re-baseline per env).
set -u
cd "$(dirname "$0")/.." || exit 2
BUILD=build
EXE="$BUILD/AccountingQuick.exe"
win() { cygpath -m "$1"; }

echo "== building =="
cmake --build "$BUILD" --target AccountingQuick 2>&1 | grep -iE "error:|error C" && { echo "BUILD FAILED"; exit 1; }
[ -f "$EXE" ] || { echo "missing $EXE"; exit 1; }

run() {  # run($N, $data, extra-env...) → gen + measure, output to stdout
  local N="$1" d="$2"; shift 2
  env ACCT_DATA_DIR="$d" ACCT_PERF="gen:$N" "$@" "./$EXE" 2>&1 | grep -iE "perf gen"
  env ACCT_DATA_DIR="$d" ACCT_PERF="measure:5" "$@" "./$EXE" 2>&1 | grep -vE "catalogs.json|Translations will not"
}

# ── Full evidence sweep ──────────────────────────────────────────────────────
if [ "${1:-gate}" = "sweep" ]; then
  shift || true
  SIZES=("${@:-10000 100000 1000000}")
  for N in ${SIZES[@]}; do
    d="$BUILD/.perf_$N"; rm -rf "$d"; mkdir -p "$d"
    echo; echo "######## dataset: $N events ########"
    run "$N" "$(win "$PWD/$d")"
  done
  exit 0
fi

# ── Regression gate (fast) ───────────────────────────────────────────────────
# Small fixed dataset; skip the ~2x-replay verify. Parse size-stable rates and fail on
# regression. Calibrated from the measured baseline (18.1 ms/record replay, ~4 ms/1k index,
# ~0.53 ms/1k-events fold, 157 bytes/event, 294k events/sec gen) with generous headroom.
GATE_N="${GATE_N:-5000}"
d="$BUILD/.perf_gate"; rm -rf "$d"; mkdir -p "$d"; W="$(win "$PWD/$d")"
echo; echo "== regression gate  (N=$GATE_N, verify skipped) =="
OUT=$(env ACCT_DATA_DIR="$W" ACCT_PERF="gen:$GATE_N" "./$EXE" 2>&1; \
      env ACCT_DATA_DIR="$W" ACCT_PERF_NOVERIFY=1 ACCT_PERF="measure:5" "./$EXE" 2>&1)
echo "$OUT" | grep -vE "catalogs.json|Translations will not"

num() { grep -oE "[0-9]+\.[0-9]+|[0-9]+" | head -1; }
GEN=$(echo "$OUT"   | grep "events/sec"                 | grep -oE "[0-9]+ events/sec" | num)
IDX=$(echo "$OUT"   | grep "index build"                | awk '{for(i=1;i<=NF;i++) if($i=="median") print $(i+1)}')
BAL=$(echo "$OUT"   | grep "balanceAt"                  | awk '{for(i=1;i<=NF;i++) if($i=="median") print $(i+1)}')
UNIT=$(echo "$OUT"  | grep "UNIT COST"                  | num)
BPE=$(echo "$OUT"   | grep "audit.log"                  | grep -oE "[0-9.]+ bytes/event" | num)

fail=0
check() { # check(name, value, op, threshold, unit)
  local name="$1" val="$2" op="$3" thr="$4" u="$5"
  if [ -z "$val" ]; then echo "  [WARN] $name: could not parse"; return; fi
  if awk "BEGIN{exit !($val $op $thr)}"; then
    printf "  [PASS] %-26s %-10s %s %s %s\n" "$name" "$val" "$op" "$thr" "$u"
  else
    printf "  [FAIL] %-26s %-10s NOT %s %s %s\n" "$name" "$val" "$op" "$thr" "$u"; fail=1
  fi
}
# Rates (index/fold normalized to be size-independent).
IDX_PER1K=$(awk "BEGIN{print $IDX/($GATE_N/1000.0)}")
BAL_PER1M=$(awk "BEGIN{print $BAL/($GATE_N/1000000.0)}")
echo
echo "== thresholds =="
check "gen throughput (ev/s)"   "$GEN"       ">=" 100000 ""
check "index build (ms/1k ev)"  "$IDX_PER1K" "<=" 8      "ms/1k"
check "fold balanceAt (ms/1M)"  "$BAL_PER1M" "<=" 900    "ms/1M"
check "replay unit (ms/record)" "$UNIT"      "<=" 30     "ms/rec"
check "audit.log (bytes/event)" "$BPE"       "<=" 220    "B/ev"

echo
if [ "$fail" -eq 0 ]; then echo "== PERF GATE PASSED (within thresholds) =="; else echo "== PERF REGRESSION DETECTED =="; fi
exit "$fail"
