#include <catch2/catch_test_macros.hpp>
#include <set>

#include "prng.hpp"

TEST_CASE("SplitMix64 matches the published reference values for seed 0", "[prng]") {
  // First three outputs of the reference C implementation (seeded with 0).
  fhtest::SplitMix64 rng(0);
  REQUIRE(rng.next() == 0xE220A8397B1DCDAFULL);
  REQUIRE(rng.next() == 0x6E789E6AA1B965F4ULL);
  REQUIRE(rng.next() == 0x06C45D188009454FULL);
}

TEST_CASE("the same seed gives the same sequence and different seeds differ", "[prng]") {
  fhtest::SplitMix64 a(42), b(42), c(43);
  for (int i = 0; i < 100; ++i) REQUIRE(a.next() == b.next());
  fhtest::SplitMix64 a2(42);
  REQUIRE(a2.next() != c.next());
}

TEST_CASE("below(n) stays inside the range and reaches every value", "[prng]") {
  fhtest::SplitMix64 rng(1);
  std::set<std::uint64_t> seen;
  for (int i = 0; i < 2000; ++i) {
    const std::uint64_t v = rng.below(6);
    REQUIRE(v < 6);
    seen.insert(v);
  }
  REQUIRE(seen.size() == 6);
}

TEST_CASE("below(1) is always 0", "[prng]") {
  fhtest::SplitMix64 rng(5);
  for (int i = 0; i < 50; ++i) REQUIRE(rng.below(1) == 0);
}

TEST_CASE("between includes both ends and handles negatives", "[prng]") {
  fhtest::SplitMix64 rng(9);
  bool saw_lo = false, saw_hi = false;
  for (int i = 0; i < 2000; ++i) {
    const std::int64_t v = rng.between(-2, 2);
    REQUIRE(v >= -2);
    REQUIRE(v <= 2);
    saw_lo = saw_lo || v == -2;
    saw_hi = saw_hi || v == 2;
  }
  REQUIRE(saw_lo);
  REQUIRE(saw_hi);
}
