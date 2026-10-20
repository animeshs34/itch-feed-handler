#include <catch2/catch_test_macros.hpp>

#include "oracle_book.hpp"

using fh::Side;

namespace {

// The ten-message example used in the walkthrough page, two instruments: 1 = X, 2 = Y.
std::vector<fh::BookEvent> walkthrough_events() {
  return {
      fh::make_add(1, 1, 101, Side::Buy, 100, 10),
      fh::make_add(2, 2, 201, Side::Sell, 50, 7),
      fh::make_add(3, 1, 102, Side::Sell, 101, 5),
      fh::make_add(4, 2, 202, Side::Sell, 50, 4),
      fh::make_reduce(5, 1, 101, 4),
      fh::make_replace(6, 1, 102, 103, 102, 8),
      fh::make_reduce(7, 2, 201, 3),
      fh::make_add(8, 1, 104, Side::Buy, 100, 6),
      fh::make_delete(9, 2, 202),
      fh::make_reduce(10, 1, 103, 8),
  };
}

}  // namespace

TEST_CASE("the oracle reproduces the walkthrough example by hand-checked values", "[oracle]") {
  oracle::Book book;
  for (const auto& e : walkthrough_events()) REQUIRE(book.apply(e));

  // X (1): order 101 has 10-4 = 6; 104 adds 6; 102 was replaced by 103, which was fully executed.
  const std::vector<oracle::Order> x = {{101, 1, Side::Buy, 100, 6}, {104, 1, Side::Buy, 100, 6}};
  REQUIRE(book.orders_of(1) == x);
  const std::vector<oracle::Level> x_levels = {{Side::Buy, 100, 12}};
  REQUIRE(book.levels_of(1) == x_levels);

  // Y (2): 201 has 7-3 = 4; 202 was deleted.
  const std::vector<oracle::Order> y = {{201, 2, Side::Sell, 50, 4}};
  REQUIRE(book.orders_of(2) == y);
  const std::vector<oracle::Level> y_levels = {{Side::Sell, 50, 4}};
  REQUIRE(book.levels_of(2) == y_levels);
}

TEST_CASE("the oracle rejects rule violations and stays unchanged", "[oracle]") {
  oracle::Book book;
  REQUIRE(book.apply(fh::make_add(1, 1, 101, Side::Buy, 100, 10)));
  REQUIRE_FALSE(book.apply(fh::make_add(2, 1, 101, Side::Buy, 100, 1)));
  REQUIRE_FALSE(book.apply(fh::make_reduce(2, 1, 101, 11)));
  REQUIRE_FALSE(book.apply(fh::make_reduce(2, 1, 999, 1)));
  REQUIRE_FALSE(book.apply(fh::make_delete(2, 1, 999)));
  REQUIRE_FALSE(book.apply(fh::make_replace(2, 1, 101, 101, 100, 1)));
  REQUIRE(book.orders_of(1).size() == 1);
  REQUIRE(book.orders_of(1)[0].shares == 10);
}
