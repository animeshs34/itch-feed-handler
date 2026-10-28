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

add_executable(fh_mktrace
  fh_mktrace.cpp)
target_link_libraries(fh_mktrace PRIVATE fh_sim)
