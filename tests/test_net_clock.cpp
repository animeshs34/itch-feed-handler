#include <catch2/catch_test_macros.hpp>
#include <thread>

#include "fh/net/clock.hpp"

TEST_CASE("the clock starts near zero and never goes backwards", "[net][clock]") {
  const fh::net::MonotonicClock clock;
  const fh::Nanos first = clock.now();
  REQUIRE(first >= 0);
  REQUIRE(first < fh::kSecond);
  fh::Nanos previous = first;
  for (int i = 0; i < 10000; ++i) {
    const fh::Nanos t = clock.now();
    REQUIRE(t >= previous);
    previous = t;
  }
}

TEST_CASE("the clock advances with real time", "[net][clock]") {
  const fh::net::MonotonicClock clock;
  const fh::Nanos before = clock.now();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  const fh::Nanos after = clock.now();
  REQUIRE(after - before >= 20 * fh::kMillisecond);
  REQUIRE(after - before < 2 * fh::kSecond);  // generous: a loaded machine may sleep longer
}

TEST_CASE("two clocks made at different times have different zeros", "[net][clock]") {
  const fh::net::MonotonicClock early;
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  const fh::net::MonotonicClock late;
  REQUIRE(early.now() > late.now());
}
