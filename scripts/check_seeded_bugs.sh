#!/usr/bin/env bash
# Checks the checker: builds the handler once per seeded bug (include/fh/seeded_bug.hpp),
# runs a campaign against it, and requires the campaign to fail.
#
#   scripts/check_seeded_bugs.sh SOURCE_DIR WORK_DIR [SCENARIOS]
#
# Extra CMake arguments (for example -DFETCHCONTENT_SOURCE_DIR_CATCH2=/catch2) come from
# the environment variable CMAKE_ARGS. Exit status: 0 if every bug was caught, 1 if any
# slipped through.
set -u

source_dir=${1:?usage: check_seeded_bugs.sh SOURCE_DIR WORK_DIR [SCENARIOS]}
work_dir=${2:?usage: check_seeded_bugs.sh SOURCE_DIR WORK_DIR [SCENARIOS]}
scenarios=${3:-300}
bugs=(1 2 3 4 5 6 7)

mkdir -p "$work_dir"

missed=0
printf '%-4s %-9s %s\n' bug result "first message"
for n in "${bugs[@]}"; do
  build="$work_dir/bug$n"
  # shellcheck disable=SC2086
  cmake -S "$source_dir" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DFH_SEEDED_BUG="$n" ${CMAKE_ARGS:-} >"$build.config.log" 2>&1 \
    && cmake --build "$build" --target fh_campaign fh_replay >"$build.build.log" 2>&1
  if [ $? -ne 0 ]; then
    printf '%-4s %-9s %s\n' "$n" "BUILD-FAIL" "see $build.build.log"
    missed=1
    continue
  fi

  out="$work_dir/bug$n.out"
  mkdir -p "$work_dir/bug$n.traces"
  "$build/tools/fh_campaign" --scenarios "$scenarios" --quiet --require-coverage \
        --out "$work_dir/bug$n.traces" >"$out" 2>&1
  status=$?
  if [ "$status" -eq 0 ]; then
    printf '%-4s %-9s %s\n' "$n" "MISSED" "campaign passed with the bug built in"
    missed=1
  else
    # Exit 1: a property failed. Exit 3: only the coverage gate tripped.
    how=properties
    [ "$status" -eq 3 ] && how=coverage
    first=$(grep -m1 -E '^    (P[0-9]|[a-z])' "$out" || true)
    [ "$status" -eq 3 ] && first=$(grep -m1 'never reached' "$out" || true)
    printf '%-4s %-9s %s\n' "$n" "caught($how)" "${first:-exit $status}"
  fi
done
exit "$missed"
