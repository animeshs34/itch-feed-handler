include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
  SYSTEM)
FetchContent_MakeAvailable(Catch2)

add_executable(fh_tests
  test_smoke.cpp
  test_types.cpp
  test_events.cpp
  test_prng.cpp
  test_level_totals.cpp
  test_order_book.cpp
  test_oracle.cpp
  test_differential.cpp
  test_state_encode.cpp
  test_state_decode.cpp
  test_checksum.cpp
  test_properties.cpp
  test_messages.cpp
  test_arbiter.cpp
  test_sequencer_order.cpp
  test_sequencer_gaps.cpp
  test_sequencer_tail.cpp
  test_line_monitor.cpp
  test_snapshot_reassembler.cpp
  test_instrument_state.cpp
  test_instrument_set.cpp
  test_core_basic.cpp
  test_core_scenarios.cpp
  test_core_random.cpp
  test_be_reader.cpp
  test_itch_layout.cpp
  test_itch_encoder.cpp
  test_itch_decoder.cpp
  test_itch_roundtrip.cpp
  test_mold_encoder.cpp
  test_moldudp64.cpp
  test_snapshot_decoder.cpp
  test_handler.cpp
  test_nasdaq_equivalence.cpp
  test_nasdaq_damaged.cpp
  test_binary_file.cpp
  test_sim_recorded.cpp
  test_sim_consistency.cpp
  test_net_clock.cpp
  test_net_udp.cpp
  test_net_pump.cpp
  test_live_loopback.cpp
  test_sim_ground_truth.cpp
  test_sim_day.cpp
  test_sim_wire.cpp
  test_sim_snapshots.cpp
  test_sim_faults.cpp
  test_sim_delivery.cpp
  test_sim_main_faults.cpp
  test_sim_snapshot_faults.cpp
  test_sim_scenario.cpp
  test_sim_checker.cpp
  test_sim_sweep.cpp
  test_core_late_join.cpp
  test_sim_trace_io.cpp
  test_sim_shrink.cpp
  test_sim_coverage.cpp
  test_sim_metamorphic.cpp
  test_sim_campaign.cpp)
target_include_directories(fh_tests PRIVATE support)
target_link_libraries(fh_tests PRIVATE fh_core fh_nasdaq fh_sim fh_net fh_live Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(fh_tests)

if(FH_FUZZ)
endif()
