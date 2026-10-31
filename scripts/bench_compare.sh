#!/usr/bin/env bash
# Compares the handler throughput of two builds of fh_bench on the same synthetic day.
# The builds run alternately (base, head, base, head, ...) so that a slow or fast stretch
# of the machine hits both. It compares medians, and only the ratio is meaningful: on a
# shared machine the absolute numbers are not.
#
#   scripts/bench_compare.sh BASE_BIN HEAD_BIN [RUNS] [EVENTS] [TOLERANCE_PERCENT]
#
# Exit status: 0 head is within the tolerance of base (or faster), 1 head is slower than
# that, 2 bad arguments or a run that failed.
set -euo pipefail
base="${1:?usage: bench_compare.sh BASE_BIN HEAD_BIN [RUNS] [EVENTS] [TOLERANCE_PERCENT]}"
head="${2:?usage: bench_compare.sh BASE_BIN HEAD_BIN [RUNS] [EVENTS] [TOLERANCE_PERCENT]}"
runs="${3:-5}"
events="${4:-300000}"
tolerance="${5:-10}"

# Messages per second of the "handler" line, from one `fh_bench closed` run.
throughput() {
  "$1" closed --events "$events" 2>/dev/null |
    awk '/^  handler / { v = $2; u = $3
                         if (u == "M/s") v *= 1e6; else if (u == "k/s") v *= 1e3
                         print v }'
}

median() { sort -g | awk '{ a[NR] = $1 } END { print a[int((NR + 1) / 2)] }'; }

base_runs=(); head_runs=()
for ((i = 0; i < runs; i++)); do
  b=$(throughput "$base"); h=$(throughput "$head")
  [[ -n "$b" && -n "$h" ]] || { echo "could not read a throughput" >&2; exit 2; }
  base_runs+=("$b"); head_runs+=("$h")
done
base_m=$(printf '%s\n' "${base_runs[@]}" | median)
head_m=$(printf '%s\n' "${head_runs[@]}" | median)

awk -v b="$base_m" -v h="$head_m" -v tol="$tolerance" 'BEGIN {
  change = (h - b) / b * 100
  printf "handler throughput (median): base %.2f M/s, head %.2f M/s (%+.1f%%), allowed -%.0f%%\n",
         b / 1e6, h / 1e6, change, tol
  exit (change < -tol) ? 1 : 0
}'
