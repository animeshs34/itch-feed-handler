cmake_minimum_required(VERSION 3.25)
project(itch-feed-handler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# 0 = the real code. 1..7 switch on one deliberate defect (include/fh/seeded_bug.hpp).
set(FH_SEEDED_BUG 0 CACHE STRING "Which seeded bug to build in; 0 for none")
option(FH_WERROR "Treat compiler warnings as errors" ON)
option(FH_BUILD_TESTS "Build the test suite" ON)
option(FH_BUILD_TOOLS "Build the command-line tools" ON)
option(FH_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(FH_FUZZ "Build the libFuzzer target (needs clang and FH_SANITIZE=ON)" OFF)

# One place that decides how strict the compiler is. Every target links it.
add_library(fh_warnings INTERFACE)
target_compile_options(fh_warnings INTERFACE
  -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
if(FH_WERROR)
  target_compile_options(fh_warnings INTERFACE -Werror)
endif()
if(FH_SANITIZE)
  target_compile_options(fh_warnings INTERFACE
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
  target_link_options(fh_warnings INTERFACE -fsanitize=address,undefined)
endif()
if(FH_FUZZ)
  # Instruments the libraries so the fuzzer can see which branches an input reaches.
  target_compile_options(fh_warnings INTERFACE -fsanitize=fuzzer-no-link)
endif()

add_library(fh_core STATIC
  src/level_totals.cpp
  src/order_book.cpp
  src/state_encode.cpp
  src/state_decode.cpp
  src/arbiter.cpp
  src/sequencer.cpp
  src/line_monitor.cpp
  src/snapshot_reassembler.cpp
  src/instrument_state.cpp
  src/instrument_set.cpp
  src/core.cpp)
target_include_directories(fh_core PUBLIC include)
target_link_libraries(fh_core PUBLIC fh_warnings)
target_compile_definitions(fh_core PUBLIC FH_SEEDED_BUG=${FH_SEEDED_BUG})

add_subdirectory(src/nasdaq)

# The test helpers (random numbers, the oracle, the consumer model) are plain headers.
# The simulator uses them too, so they get a target of their own instead of being
# reached into by path.
add_library(fh_testkit INTERFACE)
target_include_directories(fh_testkit INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tests/support)
target_link_libraries(fh_testkit INTERFACE fh_core)

add_subdirectory(src/sim)

if(FH_BUILD_TESTS)
  enable_testing()
  # The layering rule is checked as a test, so breaking it fails the build.
  add_test(NAME layer_rule
           COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/check_seams.sh ${CMAKE_SOURCE_DIR})
endif()

if(FH_BUILD_TOOLS)
endif()
if(FH_BUILD_TESTS)
  add_subdirectory(tests)
endif()
