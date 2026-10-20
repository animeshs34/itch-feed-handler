#include <catch2/catch_test_macros.hpp>

#include "fh/events.hpp"

TEST_CASE("make_add fills the fields an Add uses", "[events]") {
  const fh::BookEvent e = fh::make_add(7, 3, 101, fh::Side::Sell, 1502500, 40);
  REQUIRE(e.kind == fh::EventKind::Add);
  REQUIRE(e.seq == 7);
  REQUIRE(e.inst == 3);
  REQUIRE(e.order == 101);
  REQUIRE(e.side == fh::Side::Sell);
  REQUIRE(e.price == 1502500);
  REQUIRE(e.shares == 40);
}

TEST_CASE("make_reduce make_delete and make_replace set their kind and ids", "[events]") {
  const fh::BookEvent r = fh::make_reduce(8, 3, 101, 10);
  REQUIRE(r.kind == fh::EventKind::Reduce);
  REQUIRE(r.order == 101);
  REQUIRE(r.shares == 10);

  const fh::BookEvent d = fh::make_delete(9, 3, 101);
  REQUIRE(d.kind == fh::EventKind::Delete);
  REQUIRE(d.order == 101);

  const fh::BookEvent p = fh::make_replace(10, 3, 101, 102, 1503000, 70);
  REQUIRE(p.kind == fh::EventKind::Replace);
  REQUIRE(p.order == 101);
  REQUIRE(p.new_order == 102);
  REQUIRE(p.price == 1503000);
  REQUIRE(p.shares == 70);
}

TEST_CASE("events compare equal field by field", "[events]") {
  REQUIRE(fh::make_delete(1, 2, 3) == fh::make_delete(1, 2, 3));
  REQUIRE_FALSE(fh::make_delete(1, 2, 3) == fh::make_delete(1, 2, 4));
  REQUIRE_FALSE(fh::make_delete(1, 2, 3) == fh::make_reduce(1, 2, 3, 1));
}

TEST_CASE("events can be built in constant expressions", "[events]") {
  constexpr fh::BookEvent e = fh::make_delete(1, 2, 3);
  static_assert(e.order == 3);
  REQUIRE(e.order == 3);
}
