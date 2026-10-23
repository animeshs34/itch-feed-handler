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
  test_line_monitor.cpp)
target_include_directories(fh_tests PRIVATE support)
target_link_libraries(fh_tests PRIVATE fh_core Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(fh_tests)

if(FH_FUZZ)
endif()
