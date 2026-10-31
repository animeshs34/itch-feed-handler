#!/usr/bin/env bash
# Compares how many instructions two builds of fh_bench execute on the same synthetic day.
# Counting instructions (callgrind) gives the same answer on every run and every machine,
# so unlike a timing it can fail a build without flaking.
#
#   scripts/bench_instructions.sh BASE_BIN HEAD_BIN [TOLERANCE_PERCENT]
#
# Exit status: 0 head is within the tolerance of base (or faster), 1 head executes more
# instructions than that, 2 bad arguments or a run that failed.
set -euo pipefail
base="${1:?usage: bench_instructions.sh BASE_BIN HEAD_BIN [TOLERANCE_PERCENT]}"
head="${2:?usage: bench_instructions.sh BASE_BIN HEAD_BIN [TOLERANCE_PERCENT]}"
tolerance="${3:-2}"

count() {
  local out
  out=$(valgrind --tool=callgrind --callgrind-out-file=/dev/null \
          "$1" closed --events 20000 --repeats 1 2>&1) || { echo "run failed: $1" >&2; exit 2; }
  # callgrind prints "Collected : <instructions>" once, at the end.
  awk '/Collected :/ { print $4 }' <<<"$out" | tail -n 1
}

base_n=$(count "$base")
head_n=$(count "$head")
[[ -n "$base_n" && -n "$head_n" ]] || { echo "could not read an instruction count" >&2; exit 2; }

awk -v b="$base_n" -v h="$head_n" -v tol="$tolerance" 'BEGIN {
  change = (h - b) / b * 100
  printf "instructions: base %d, head %d (%+.2f%%), allowed +%.1f%%\n", b, h, change, tol
  exit (change > tol) ? 1 : 0
}'
