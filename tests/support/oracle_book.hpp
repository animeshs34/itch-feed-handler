#ifndef FH_TESTS_SUPPORT_ORACLE_BOOK_HPP_
#define FH_TESTS_SUPPORT_ORACLE_BOOK_HPP_

#include <algorithm>
#include <vector>

#include "fh/events.hpp"
#include "fh/types.hpp"

namespace oracle {

// The answer key. Deliberately naive and deliberately different from
// fh::InstrumentBook: one flat vector, linear searches, levels found by
// scanning. It shares only the plain vocabulary types (ids, Side, BookEvent)
// with the real code, never any logic, so a bug cannot hide in both.
//
// It does not implement the overflow rule: tests that need it use the real
// book directly with extreme values.

struct Order {
  fh::OrderId id;
  fh::InstrumentId inst;
  fh::Side side;
  fh::Price price;
  fh::Shares shares;

  friend bool operator==(const Order&, const Order&) = default;
};

struct Level {
  fh::Side side;
  fh::Price price;
  fh::Shares total;

  friend bool operator==(const Level&, const Level&) = default;
};

class Book {
 public:
  // Applies the event. Returns false, leaving the book untouched, if the event
  // breaks a rule (unknown order, too many shares, reused id, zero shares).
  bool apply(const fh::BookEvent& e) {
    switch (e.kind) {
      case fh::EventKind::Add:
        if (e.shares == 0 || find(e.order) != all_.end()) return false;
        all_.push_back(Order{e.order, e.inst, e.side, e.price, e.shares});
        return true;
      case fh::EventKind::Reduce: {
        auto it = find(e.order);
        if (e.shares == 0 || it == all_.end() || e.shares > it->shares) return false;
        it->shares -= e.shares;
        if (it->shares == 0) all_.erase(it);
        return true;
      }
      case fh::EventKind::Delete: {
        auto it = find(e.order);
        if (it == all_.end()) return false;
        all_.erase(it);
        return true;
      }
      case fh::EventKind::Replace: {
        auto it = find(e.order);
        if (e.shares == 0 || it == all_.end() || find(e.new_order) != all_.end()) return false;
        const Order old = *it;
        all_.erase(it);
        all_.push_back(Order{e.new_order, old.inst, old.side, e.price, e.shares});
        return true;
      }
    }
    return false;
  }

  // Orders of one instrument, sorted by id.
  std::vector<Order> orders_of(fh::InstrumentId inst) const {
    std::vector<Order> out;
    for (const Order& o : all_) {
      if (o.inst == inst) out.push_back(o);
    }
    std::sort(out.begin(), out.end(), [](const Order& a, const Order& b) { return a.id < b.id; });
    return out;
  }

  // Price levels of one instrument, found by scanning the orders, sorted by (side, price).
  std::vector<Level> levels_of(fh::InstrumentId inst) const {
    std::vector<Level> out;
    for (const Order& o : orders_of(inst)) {
      auto it = std::find_if(out.begin(), out.end(), [&](const Level& l) {
        return l.side == o.side && l.price == o.price;
      });
      if (it == out.end()) {
        out.push_back(Level{o.side, o.price, o.shares});
      } else {
        it->total += o.shares;
      }
    }
    std::sort(out.begin(), out.end(), [](const Level& a, const Level& b) {
      if (a.side != b.side) return a.side < b.side;
      return a.price < b.price;
    });
    return out;
  }

 private:
  std::vector<Order>::iterator find(fh::OrderId id) {
    return std::find_if(all_.begin(), all_.end(), [&](const Order& o) { return o.id == id; });
  }

  std::vector<Order> all_;
};

}  // namespace oracle

#endif  // FH_TESTS_SUPPORT_ORACLE_BOOK_HPP_
