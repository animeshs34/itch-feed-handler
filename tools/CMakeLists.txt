add_executable(fh_campaign
  fh_campaign.cpp)
target_link_libraries(fh_campaign PRIVATE fh_sim)

add_executable(fh_replay
  fh_replay.cpp)
target_link_libraries(fh_replay PRIVATE fh_sim)

add_executable(fh_mkfile
  fh_mkfile.cpp)
target_link_libraries(fh_mkfile PRIVATE fh_sim)

add_executable(fh_inspect
  fh_inspect.cpp)
target_link_libraries(fh_inspect PRIVATE fh_sim)

# Command-line smoke tests: the tools run, and they report their result in the exit status.
add_test(NAME fh_campaign_passes_a_small_campaign
         COMMAND fh_campaign --scenarios 25 --quiet)
add_test(NAME fh_campaign_refuses_bad_arguments
         COMMAND fh_campaign --scenarios 0)
set_tests_properties(fh_campaign_refuses_bad_arguments PROPERTIES WILL_FAIL TRUE)
add_test(NAME fh_replay_refuses_a_missing_file
         COMMAND fh_replay no/such/file.fhtr)
set_tests_properties(fh_replay_refuses_a_missing_file PROPERTIES WILL_FAIL TRUE)

# The recorded-file chain, with a generated file standing in for a real one: write it,
# inspect it, run a campaign on it. An incomplete file (no terminator) must be refused.
set(FH_SAMPLE_FILE ${CMAKE_CURRENT_BINARY_DIR}/sample_day.bin)
set(FH_CUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/cut_day.bin)
add_test(NAME fh_mkfile_writes_a_day
         COMMAND fh_mkfile --seed 4 --events 1500 ${FH_SAMPLE_FILE})
add_test(NAME fh_mkfile_writes_an_incomplete_day
         COMMAND fh_mkfile --seed 4 --events 1500 --no-terminator ${FH_CUT_FILE})
set_tests_properties(fh_mkfile_writes_a_day fh_mkfile_writes_an_incomplete_day
                     PROPERTIES FIXTURES_SETUP sample_files)

add_test(NAME fh_inspect_accepts_the_day COMMAND fh_inspect ${FH_SAMPLE_FILE})
add_test(NAME fh_inspect_refuses_the_incomplete_day COMMAND fh_inspect ${FH_CUT_FILE})
set_tests_properties(fh_inspect_refuses_the_incomplete_day PROPERTIES WILL_FAIL TRUE)
add_test(NAME fh_campaign_runs_on_a_recorded_day
         COMMAND fh_campaign --file ${FH_SAMPLE_FILE} --instruments 1,2,3 --scenarios 20 --quiet)
add_test(NAME fh_campaign_refuses_the_incomplete_day
         COMMAND fh_campaign --file ${FH_CUT_FILE} --scenarios 5 --quiet)
set_tests_properties(fh_campaign_refuses_the_incomplete_day PROPERTIES WILL_FAIL TRUE)
set_tests_properties(fh_inspect_accepts_the_day fh_inspect_refuses_the_incomplete_day
                     fh_campaign_runs_on_a_recorded_day fh_campaign_refuses_the_incomplete_day
                     PROPERTIES FIXTURES_REQUIRED sample_files)

add_executable(fh_send
  fh_send.cpp)
target_link_libraries(fh_send PRIVATE fh_live)

# The library is already called fh_live, so the executable target has another name and
# the same file name on disk.
add_executable(fh_live_tool
  fh_live.cpp)
target_link_libraries(fh_live_tool PRIVATE fh_live)
set_target_properties(fh_live_tool PROPERTIES OUTPUT_NAME fh_live)

add_executable(fh_mktrace
  fh_mktrace.cpp)
target_link_libraries(fh_mktrace PRIVATE fh_sim)

# Two real processes over real sockets: fh_live listens, fh_send plays a trace, and the
# trace fh_live recorded must pass the same checks the trace it was sent does.
add_test(NAME loopback_tools_agree
         COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/loopback_check.sh ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(loopback_tools_agree PROPERTIES TIMEOUT 60)

add_executable(fh_bench_tool
  fh_bench.cpp)
target_link_libraries(fh_bench_tool PRIVATE fh_bench)
set_target_properties(fh_bench_tool PROPERTIES OUTPUT_NAME fh_bench)

# Smoke tests: the benchmark tool runs, prints its environment, and refuses nonsense. The
# numbers it prints are for a Release build on a quiet machine, not for a test.
add_test(NAME fh_bench_runs_a_short_closed_loop
         COMMAND fh_bench closed --events 20000 --repeats 1)
add_test(NAME fh_bench_measures_recovery
         COMMAND fh_bench recovery --tier moderate --scenarios 20)
add_test(NAME fh_bench_prints_the_environment COMMAND fh_bench env)
set_tests_properties(fh_bench_prints_the_environment PROPERTIES PASS_REGULAR_EXPRESSION "environment:")
add_test(NAME fh_bench_refuses_a_bad_command COMMAND fh_bench nonsense)
set_tests_properties(fh_bench_refuses_a_bad_command PROPERTIES WILL_FAIL TRUE)
