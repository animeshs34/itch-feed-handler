#ifndef FH_ORDER_BOOK_HPP_
#define FH_ORDER_BOOK_HPP_

#include <array>
#include <cstdint>
#include <map>
#include <optional>

#include "fh/events.hpp"
#include "fh/level_totals.hpp"
#include "fh/types.hpp"

namespace fh {

// One live order. The order id is the key of the map that holds it.
struct Order {
  Side side{Side::Buy};
  Price price{0};
  Shares shares{0};

  friend constexpr bool operator==(const Order&, const Order&) = default;
};

// Why an event could not be applied. None means it was applied.
enum class ApplyError : std::uint8_t {
  None,
  ZeroShares,      // an event asked for zero shares
  DuplicateOrder,  // Add: the order id is already live
  UnknownOrder,    // Reduce, Delete, Replace: no such live order
  ReduceTooLarge,  // Reduce: more shares than the order has left
  NewOrderExists,  // Replace: the new id is already live (or equals the old id)
  LevelOverflow,   // a price level total would exceed the Shares range
};

// A price level an event changed, with its total after the change.
// total_after == 0 means the level no longer exists.
struct TouchedLevel {
  LevelKey key{};
  Shares total_after{0};

  friend constexpr bool operator==(const TouchedLevel&, const TouchedLevel&) = default;
};

// The outcome of apply(). On error nothing in the book has changed.
struct ApplyResult {
  ApplyError error{ApplyError::None};
  std::array<TouchedLevel, 2> touched{};
  std::uint8_t touched_count{0};

  [[nodiscard]] bool ok() const noexcept { return error == ApplyError::None; }
};

// The live orders of one instrument plus the price-level totals derived from
// them. The class has an invariant (levels_ always equals the totals of
// orders_), so members are private and every change goes through apply().
class InstrumentBook {
 public:
  // Applies one event. The caller must look at the result: a failed event
  // means the book cannot be trusted to keep following the feed.
  [[nodiscard]] ApplyResult apply(const BookEvent& event);

  // Builds a book from a list of orders, for loading snapshots. Returns
  // nullopt if any order has zero shares or a level total would overflow.
  [[nodiscard]] static std::optional<InstrumentBook> from_orders(std::map<OrderId, Order> orders);

  // Live orders sorted by order id.
  [[nodiscard]] const std::map<OrderId, Order>& orders() const noexcept { return orders_; }

  // Price-level totals worked out from the orders.
  [[nodiscard]] const LevelTotals& levels() const noexcept { return levels_; }

  // Two books are equal when their orders are equal; the levels follow.
  friend bool operator==(const InstrumentBook& a, const InstrumentBook& b) {
    return a.orders_ == b.orders_;
  }

 private:
  [[nodiscard]] ApplyResult apply_add(const BookEvent& event);
  [[nodiscard]] ApplyResult apply_reduce(const BookEvent& event);
  [[nodiscard]] ApplyResult apply_delete(const BookEvent& event);
  [[nodiscard]] ApplyResult apply_replace(const BookEvent& event);

  std::map<OrderId, Order> orders_;
  LevelTotals levels_;
};

}  // namespace fh

#endif  // FH_ORDER_BOOK_HPP_
