#include <catch2/catch_test_macros.hpp>
#include <limits>

#include "fh/level_totals.hpp"

namespace {
const fh::LevelKey kBid100{fh::Side::Buy, 100};
const fh::LevelKey kAsk100{fh::Side::Sell, 100};
}  // namespace

TEST_CASE("an empty LevelTotals has no levels", "[levels]") {
  fh::LevelTotals levels;
  REQUIRE(levels.total(kBid100) == 0);
  REQUIRE(levels.all().empty());
}

TEST_CASE("add accumulates and sides are separate levels", "[levels]") {
  fh::LevelTotals levels;
  levels.add(kBid100, 10);
  levels.add(kBid100, 5);
  levels.add(kAsk100, 7);
  REQUIRE(levels.total(kBid100) == 15);
  REQUIRE(levels.total(kAsk100) == 7);
  REQUIRE(levels.all().size() == 2);
}

TEST_CASE("sub reduces a level and erases it at zero", "[levels]") {
  fh::LevelTotals levels;
  levels.add(kBid100, 10);
  levels.sub(kBid100, 4);
  REQUIRE(levels.total(kBid100) == 6);
  levels.sub(kBid100, 6);
  REQUIRE(levels.total(kBid100) == 0);
  REQUIRE(levels.all().empty());  // erased, not stored as zero
}

TEST_CASE("levels are ordered by side then price", "[levels]") {
  fh::LevelTotals levels;
  levels.add({fh::Side::Sell, 90}, 1);
  levels.add({fh::Side::Buy, 120}, 1);
  levels.add({fh::Side::Buy, 100}, 1);
  const auto sorted = levels.all();  // all() builds a fresh sorted copy
  auto it = sorted.begin();
  REQUIRE(it->first == fh::LevelKey{fh::Side::Buy, 100});
  ++it;
  REQUIRE(it->first == fh::LevelKey{fh::Side::Buy, 120});
  ++it;
  REQUIRE(it->first == fh::LevelKey{fh::Side::Sell, 90});
}

TEST_CASE("can_add refuses a total that would overflow", "[levels]") {
  constexpr fh::Shares kMax = std::numeric_limits<fh::Shares>::max();
  fh::LevelTotals levels;
  levels.add(kBid100, kMax - 1);
  REQUIRE(levels.can_add(kBid100, 1));
  REQUIRE_FALSE(levels.can_add(kBid100, 2));
  REQUIRE(levels.can_add(kAsk100, kMax));  // another level is unaffected
}

TEST_CASE("negative prices are ordinary levels", "[levels]") {
  fh::LevelTotals levels;
  levels.add({fh::Side::Buy, -5}, 3);
  REQUIRE(levels.total({fh::Side::Buy, -5}) == 3);
}
