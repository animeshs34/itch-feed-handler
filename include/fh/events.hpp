#ifndef FH_EVENTS_HPP_
#define FH_EVENTS_HPP_

#include <cstdint>

#include "fh/types.hpp"

namespace fh {

// What the order book needs to know, stripped of any exchange's wire format.
// ITCH "executed" and "cancel" both mean "remove some shares" and so both
// become Reduce here.
enum class EventKind : std::uint8_t { Add, Reduce, Delete, Replace };

// One change to one instrument's book. Which fields are meaningful depends on
// `kind`; the make_* functions below are the only intended way to build one.
//
//   Add     order = new id, side, price, shares = total shares
//   Reduce  order = target, shares = amount to remove
//   Delete  order = target
//   Replace order = old id, new_order = new id, price, shares = new total
//           (the side is inherited from the old order)
struct BookEvent {
  EventKind kind{EventKind::Add};
  Seq seq{0};
  InstrumentId inst{0};
  OrderId order{0};
  OrderId new_order{0};
  Side side{Side::Buy};
  Price price{0};
  Shares shares{0};

  friend constexpr bool operator==(const BookEvent&, const BookEvent&) = default;
};

constexpr BookEvent make_add(Seq seq, InstrumentId inst, OrderId order, Side side, Price price,
                             Shares shares) noexcept {
  BookEvent e;
  e.kind = EventKind::Add;
  e.seq = seq;
  e.inst = inst;
  e.order = order;
  e.side = side;
  e.price = price;
  e.shares = shares;
  return e;
}

constexpr BookEvent make_reduce(Seq seq, InstrumentId inst, OrderId order, Shares shares) noexcept {
  BookEvent e;
  e.kind = EventKind::Reduce;
  e.seq = seq;
  e.inst = inst;
  e.order = order;
  e.shares = shares;
  return e;
}

constexpr BookEvent make_delete(Seq seq, InstrumentId inst, OrderId order) noexcept {
  BookEvent e;
  e.kind = EventKind::Delete;
  e.seq = seq;
  e.inst = inst;
  e.order = order;
  return e;
}

constexpr BookEvent make_replace(Seq seq, InstrumentId inst, OrderId old_order, OrderId new_order,
                                 Price price, Shares shares) noexcept {
  BookEvent e;
  e.kind = EventKind::Replace;
  e.seq = seq;
  e.inst = inst;
  e.order = old_order;
  e.new_order = new_order;
  e.price = price;
  e.shares = shares;
  return e;
}

}  // namespace fh

#endif  // FH_EVENTS_HPP_
