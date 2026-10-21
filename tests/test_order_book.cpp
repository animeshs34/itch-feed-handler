#include <catch2/catch_test_macros.hpp>
#include <limits>

#include "fh/order_book.hpp"

using fh::ApplyError;
using fh::InstrumentBook;
using fh::LevelKey;
using fh::Side;

namespace {
constexpr fh::InstrumentId kInst = 1;

// Applies an event and requires it to succeed.
fh::ApplyResult must_apply(InstrumentBook& book, const fh::BookEvent& e) {
  const fh::ApplyResult r = book.apply(e);
  REQUIRE(r.ok());
  return r;
}
}  // namespace

TEST_CASE("Add inserts an order and its level", "[book]") {
  InstrumentBook book;
  const auto r = must_apply(book, fh::make_add(1, kInst, 101, Side::Buy, 100, 10));
  REQUIRE(book.orders().size() == 1);
  REQUIRE(book.orders().at(101) == fh::Order{Side::Buy, 100, 10});
  REQUIRE(book.levels().total({Side::Buy, 100}) == 10);
  REQUIRE(r.touched_count == 1);
  REQUIRE(r.touched[0].key == LevelKey{Side::Buy, 100});
  REQUIRE(r.touched[0].total_after == 10);
}

TEST_CASE("two orders at one price share a level", "[book]") {
  InstrumentBook book;
  must_apply(book, fh::make_add(1, kInst, 201, Side::Sell, 50, 7));
  const auto r = must_apply(book, fh::make_add(2, kInst, 202, Side::Sell, 50, 4));
  REQUIRE(book.levels().total({Side::Sell, 50}) == 11);
  REQUIRE(r.touched[0].total_after == 11);
}

TEST_CASE("Reduce takes shares off an order and its level", "[book]") {
  InstrumentBook book;
  must_apply(book, fh::make_add(1, kInst, 101, Side::Buy, 100, 10));
  const auto r = must_apply(book, fh::make_reduce(2, kInst, 101, 4));
  REQUIRE(book.orders().at(101).shares == 6);
  REQUIRE(book.levels().total({Side::Buy, 100}) == 6);
  REQUIRE(r.touched[0].total_after == 6);
}

TEST_CASE("Reduce to zero removes the order and the level", "[book]") {
  InstrumentBook book;
  must_apply(book, fh::make_add(1, kInst, 101, Side::Buy, 100, 10));
  const auto r = must_apply(book, fh::make_reduce(2, kInst, 101, 10));
  REQUIRE(book.orders().empty());
  REQUIRE(book.levels().all().empty());
  REQUIRE(r.touched[0].total_after == 0);
}

TEST_CASE("Delete removes the whole order", "[book]") {
  InstrumentBook book;
  must_apply(book, fh::make_add(1, kInst, 101, Side::Buy, 100, 10));
  must_apply(book, fh::make_add(2, kInst, 102, Side::Buy, 100, 5));
  const auto r = must_apply(book, fh::make_delete(3, kInst, 101));
  REQUIRE(book.orders().size() == 1);
  REQUIRE(book.levels().total({Side::Buy, 100}) == 5);
  REQUIRE(r.touched[0].total_after == 5);
}

TEST_CASE("Replace swaps the id and keeps the side and changes price and shares", "[book]") {
  InstrumentBook book;
  must_apply(book, fh::make_add(1, kInst, 102, Side::Sell, 101, 5));
  const auto r = must_apply(book, fh::make_replace(2, kInst, 102, 103, 102, 8));
  REQUIRE_FALSE(book.orders().contains(102));
  REQUIRE(book.orders().at(103) == fh::Order{Side::Sell, 102, 8});
  REQUIRE(book.levels().total({Side::Sell, 101}) == 0);
  REQUIRE(book.levels().total({Side::Sell, 102}) == 8);
  REQUIRE(r.touched_count == 2);
  REQUIRE(r.touched[0].key == LevelKey{Side::Sell, 101});
  REQUIRE(r.touched[0].total_after == 0);
  REQUIRE(r.touched[1].key == LevelKey{Side::Sell, 102});
  REQUIRE(r.touched[1].total_after == 8);
}

TEST_CASE("Replace at the same price reports one touched level", "[book]") {
  InstrumentBook book;
  must_apply(book, fh::make_add(1, kInst, 101, Side::Buy, 100, 10));
  must_apply(book, fh::make_add(2, kInst, 102, Side::Buy, 100, 5));
  const auto r = must_apply(book, fh::make_replace(3, kInst, 101, 103, 100, 20));
  REQUIRE(r.touched_count == 1);
  REQUIRE(r.touched[0].total_after == 25);  // 5 left from 102, plus 20 from 103
}

namespace {
// A rejected event must change nothing at all. This helper checks that.
void require_rejected_without_change(InstrumentBook& book, const fh::BookEvent& e,
                                     ApplyError expected) {
  const InstrumentBook before = book;
  const std::map<LevelKey, fh::Shares> levels_before = book.levels().all();
  const fh::ApplyResult r = book.apply(e);
  REQUIRE(r.error == expected);
  REQUIRE_FALSE(r.ok());
  REQUIRE(r.touched_count == 0);
  REQUIRE(book == before);
  REQUIRE(book.levels().all() == levels_before);
}
}  // namespace

TEST_CASE("every rule violation is rejected and leaves the book unchanged", "[book]") {
  InstrumentBook book;
  must_apply(book, fh::make_add(1, kInst, 101, Side::Buy, 100, 10));
  must_apply(book, fh::make_add(2, kInst, 102, Side::Sell, 101, 5));

  require_rejected_without_change(book, fh::make_add(3, kInst, 103, Side::Buy, 100, 0),
                                  ApplyError::ZeroShares);
  require_rejected_without_change(book, fh::make_add(3, kInst, 101, Side::Buy, 100, 1),
                                  ApplyError::DuplicateOrder);
  require_rejected_without_change(book, fh::make_reduce(3, kInst, 999, 1),
                                  ApplyError::UnknownOrder);
  require_rejected_without_change(book, fh::make_reduce(3, kInst, 101, 0), ApplyError::ZeroShares);
  require_rejected_without_change(book, fh::make_reduce(3, kInst, 101, 11),
                                  ApplyError::ReduceTooLarge);
  require_rejected_without_change(book, fh::make_delete(3, kInst, 999), ApplyError::UnknownOrder);
  require_rejected_without_change(book, fh::make_replace(3, kInst, 999, 500, 100, 1),
                                  ApplyError::UnknownOrder);
  require_rejected_without_change(book, fh::make_replace(3, kInst, 101, 102, 100, 1),
                                  ApplyError::NewOrderExists);
  require_rejected_without_change(book, fh::make_replace(3, kInst, 101, 101, 100, 1),
                                  ApplyError::NewOrderExists);  // own id
  require_rejected_without_change(book, fh::make_replace(3, kInst, 101, 500, 100, 0),
                                  ApplyError::ZeroShares);
}

TEST_CASE("a level total that would overflow is rejected without change", "[book]") {
  constexpr fh::Shares kMax = std::numeric_limits<fh::Shares>::max();
  InstrumentBook book;
  must_apply(book, fh::make_add(1, kInst, 1, Side::Buy, 100, kMax));

  require_rejected_without_change(book, fh::make_add(2, kInst, 2, Side::Buy, 100, 1),
                                  ApplyError::LevelOverflow);
  must_apply(book, fh::make_add(2, kInst, 2, Side::Buy, 101, 1));
  // Moving order 2 onto the full level must overflow...
  require_rejected_without_change(book, fh::make_replace(3, kInst, 2, 3, 100, 1),
                                  ApplyError::LevelOverflow);
  // ...but replacing the full order at its own price is fine: it leaves first.
  must_apply(book, fh::make_replace(3, kInst, 1, 4, 100, kMax));
  REQUIRE(book.levels().total({Side::Buy, 100}) == kMax);
}

TEST_CASE("extreme prices are ordinary prices", "[book]") {
  InstrumentBook book;
  must_apply(book, fh::make_add(1, kInst, 1, Side::Buy, std::numeric_limits<fh::Price>::min(), 1));
  must_apply(book, fh::make_add(2, kInst, 2, Side::Sell, std::numeric_limits<fh::Price>::max(), 1));
  must_apply(book, fh::make_add(3, kInst, 3, Side::Buy, 0, 1));
  REQUIRE(book.levels().all().size() == 3);
}

TEST_CASE("from_orders builds a book and rejects bad input", "[book]") {
  std::map<fh::OrderId, fh::Order> orders;
  orders.emplace(1, fh::Order{Side::Buy, 100, 10});
  orders.emplace(2, fh::Order{Side::Buy, 100, 5});
  const auto book = InstrumentBook::from_orders(orders);
  REQUIRE(book.has_value());
  REQUIRE(book->levels().total({Side::Buy, 100}) == 15);

  std::map<fh::OrderId, fh::Order> zero;
  zero.emplace(1, fh::Order{Side::Buy, 100, 0});
  REQUIRE_FALSE(InstrumentBook::from_orders(zero).has_value());

  constexpr fh::Shares kMax = std::numeric_limits<fh::Shares>::max();
  std::map<fh::OrderId, fh::Order> overflow;
  overflow.emplace(1, fh::Order{Side::Buy, 100, kMax});
  overflow.emplace(2, fh::Order{Side::Buy, 100, 1});
  REQUIRE_FALSE(InstrumentBook::from_orders(overflow).has_value());
}

TEST_CASE("the level cache always equals a recomputation from the orders", "[book]") {
  InstrumentBook book;
  must_apply(book, fh::make_add(1, kInst, 1, Side::Buy, 100, 10));
  must_apply(book, fh::make_add(2, kInst, 2, Side::Sell, 101, 5));
  must_apply(book, fh::make_reduce(3, kInst, 1, 3));
  must_apply(book, fh::make_replace(4, kInst, 2, 3, 99, 8));
  must_apply(book, fh::make_delete(5, kInst, 1));

  std::map<LevelKey, fh::Shares> recomputed;
  for (const auto& [id, order] : book.orders()) {
    recomputed[{order.side, order.price}] += order.shares;
  }
  REQUIRE(book.levels().all() == recomputed);
}
