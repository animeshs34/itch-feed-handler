#include <catch2/catch_test_macros.hpp>
#include <string_view>
#include <vector>

#include "feed_state_builder.hpp"
#include "fh/checksum.hpp"

namespace {
std::uint64_t fnv(std::string_view text) {
  std::vector<std::uint8_t> bytes(text.begin(), text.end());
  return fh::fnv1a64(bytes);
}
}  // namespace

TEST_CASE("fnv1a64 matches the published test vectors", "[checksum]") {
  REQUIRE(fnv("") == 0xcbf29ce484222325ULL);
  REQUIRE(fnv("a") == 0xaf63dc4c8601ec8cULL);
  REQUIRE(fnv("foobar") == 0x85944171f73967e8ULL);
}

TEST_CASE("fnv1a64 can be evaluated at compile time", "[checksum]") {
  constexpr std::uint8_t data[] = {'a'};
  constexpr std::uint64_t h = fh::fnv1a64(data);
  static_assert(h == 0xaf63dc4c8601ec8cULL);
  REQUIRE(h == 0xaf63dc4c8601ec8cULL);
}

TEST_CASE("equal states have equal checksums and any change alters it", "[checksum]") {
  fh::FeedState a, b;
  REQUIRE(fhtest::build_state({fh::make_add(1, 1, 101, fh::Side::Buy, 100, 10)}, a));
  REQUIRE(fhtest::build_state({fh::make_add(1, 1, 101, fh::Side::Buy, 100, 10)}, b));
  REQUIRE(fh::checksum(a) == fh::checksum(b));

  fh::FeedState other_shares, other_price, other_side, other_seq;
  REQUIRE(fhtest::build_state({fh::make_add(1, 1, 101, fh::Side::Buy, 100, 11)}, other_shares));
  REQUIRE(fhtest::build_state({fh::make_add(1, 1, 101, fh::Side::Buy, 101, 10)}, other_price));
  REQUIRE(fhtest::build_state({fh::make_add(1, 1, 101, fh::Side::Sell, 100, 10)}, other_side));
  REQUIRE(fhtest::build_state({fh::make_add(2, 1, 101, fh::Side::Buy, 100, 10)}, other_seq));
  REQUIRE(fh::checksum(a) != fh::checksum(other_shares));
  REQUIRE(fh::checksum(a) != fh::checksum(other_price));
  REQUIRE(fh::checksum(a) != fh::checksum(other_side));
  REQUIRE(fh::checksum(a) != fh::checksum(other_seq));
}

TEST_CASE("the exponent is part of the fingerprint", "[checksum]") {
  fh::FeedState a, b;
  b.price_exp = -2;
  REQUIRE(fh::checksum(a) != fh::checksum(b));
}

TEST_CASE("the checksum does not depend on how the state was reached", "[checksum]") {
  // Same final book, built two ways. Book(N) depends on the messages, not on history.
  fh::FeedState first_then_second, second_then_first;
  REQUIRE(fhtest::build_state({fh::make_add(1, 1, 101, fh::Side::Buy, 100, 10),
                               fh::make_add(2, 1, 102, fh::Side::Sell, 101, 5)},
                              first_then_second));
  REQUIRE(fhtest::build_state({fh::make_add(1, 1, 102, fh::Side::Sell, 101, 5),
                               fh::make_add(2, 1, 101, fh::Side::Buy, 100, 10)},
                              second_then_first));
  REQUIRE(fh::checksum(first_then_second) == fh::checksum(second_then_first));

  // And a book that went through extra steps and arrived at the same orders.
  fh::FeedState detour;
  REQUIRE(fhtest::build_state(
      {fh::make_add(1, 1, 101, fh::Side::Buy, 100, 99), fh::make_reduce(2, 1, 101, 89),
       fh::make_add(3, 1, 102, fh::Side::Sell, 101, 5),
       fh::make_add(4, 1, 103, fh::Side::Buy, 50, 1), fh::make_delete(5, 1, 103)},
      detour));
  detour.seq = first_then_second.seq;
  REQUIRE(fh::checksum(detour) == fh::checksum(first_then_second));
}
