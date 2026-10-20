#include <catch2/catch_test_macros.hpp>
#include <type_traits>

#include "fh/types.hpp"

// These checks run at compile time: if someone changes a type's width or
// signedness, the build fails before any test runs.
static_assert(sizeof(fh::Seq) == 8 && std::is_unsigned_v<fh::Seq>);
static_assert(sizeof(fh::InstrumentId) == 2 && std::is_unsigned_v<fh::InstrumentId>);
static_assert(sizeof(fh::OrderId) == 8 && std::is_unsigned_v<fh::OrderId>);
static_assert(sizeof(fh::Price) == 8 && std::is_signed_v<fh::Price>);
static_assert(sizeof(fh::Shares) == 8 && std::is_unsigned_v<fh::Shares>);
static_assert(sizeof(fh::PriceExp) == 1 && std::is_signed_v<fh::PriceExp>);
static_assert(sizeof(fh::Side) == 1);

TEST_CASE("Side uses the ASCII codes ITCH uses", "[types]") {
  REQUIRE(static_cast<std::uint8_t>(fh::Side::Buy) == 'B');
  REQUIRE(static_cast<std::uint8_t>(fh::Side::Sell) == 'S');
}

TEST_CASE("is_valid_side accepts only B and S", "[types]") {
  REQUIRE(fh::is_valid_side('B'));
  REQUIRE(fh::is_valid_side('S'));
  REQUIRE_FALSE(fh::is_valid_side('b'));
  REQUIRE_FALSE(fh::is_valid_side(0));
  REQUIRE_FALSE(fh::is_valid_side(255));
}
