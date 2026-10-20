#include <catch2/catch_test_macros.hpp>

// Proves three things at once: the compiler accepts C++20, the test runner is
// wired up, and the fh_core library links into the test binary.
TEST_CASE("toolchain builds and runs a C++20 test", "[smoke]") {
  static_assert(__cplusplus >= 202002L, "this project requires C++20");
  REQUIRE(1 + 1 == 2);
}
