#include "fh/order_book.hpp"

#include <limits>
#include <utility>

#include "fh/seeded_bug.hpp"

namespace fh {

namespace {

ApplyResult failure(ApplyError error) {
  ApplyResult result;
  result.error = error;
  return result;
}

}  // namespace

ApplyResult InstrumentBook::apply(const BookEvent& event) {
  // No `default:` on purpose: with every enumerator listed, the compiler warns
  // (and -Werror stops the build) the day a new EventKind is added and not handled here.
  switch (event.kind) {
    case EventKind::Add:
      return apply_add(event);
    case EventKind::Reduce:
      return apply_reduce(event);
    case EventKind::Delete:
      return apply_delete(event);
    case EventKind::Replace:
      return apply_replace(event);
  }
  return failure(ApplyError::UnknownOrder);  // unreachable; keeps the compiler content
}

ApplyResult InstrumentBook::apply_add(const BookEvent& event) {
  if (event.shares == 0) return failure(ApplyError::ZeroShares);
  if (orders_.contains(event.order)) return failure(ApplyError::DuplicateOrder);
  const LevelKey key{event.side, event.price};
  if (!levels_.can_add(key, event.shares)) return failure(ApplyError::LevelOverflow);

  // Every check has passed; from here on nothing can fail.
  orders_.emplace(event.order, Order{event.side, event.price, event.shares});
  levels_.add(key, event.shares);

  ApplyResult result;
  result.touched[0] = TouchedLevel{key, levels_.total(key)};
  result.touched_count = 1;
  return result;
}

ApplyResult InstrumentBook::apply_reduce(const BookEvent& event) {
  if (event.shares == 0) return failure(ApplyError::ZeroShares);
  const auto it = orders_.find(event.order);
  if (it == orders_.end()) return failure(ApplyError::UnknownOrder);
  if (event.shares > it->second.shares) return failure(ApplyError::ReduceTooLarge);

  const LevelKey key{it->second.side, it->second.price};
  levels_.sub(key, event.shares);
  it->second.shares -= event.shares;
  if (it->second.shares == 0 && !FH_BUG(6)) {
    orders_.erase(it);  // a fully used-up order is dead (ITCH 5.0, section 1.4)
  }

  ApplyResult result;
  result.touched[0] = TouchedLevel{key, levels_.total(key)};
  result.touched_count = 1;
  return result;
}

ApplyResult InstrumentBook::apply_delete(const BookEvent& event) {
  const auto it = orders_.find(event.order);
  if (it == orders_.end()) return failure(ApplyError::UnknownOrder);

  const LevelKey key{it->second.side, it->second.price};
  levels_.sub(key, it->second.shares);
  orders_.erase(it);

  ApplyResult result;
  result.touched[0] = TouchedLevel{key, levels_.total(key)};
  result.touched_count = 1;
  return result;
}

ApplyResult InstrumentBook::apply_replace(const BookEvent& event) {
  if (event.shares == 0) return failure(ApplyError::ZeroShares);
  const auto it = orders_.find(event.order);
  if (it == orders_.end()) return failure(ApplyError::UnknownOrder);
  // Checked against the table before the old order is removed, so replacing an
  // order with its own id is rejected rather than silently allowed.
  if (orders_.contains(event.new_order)) return failure(ApplyError::NewOrderExists);

  const Order old_order = it->second;
  const LevelKey old_key{old_order.side, old_order.price};
  const LevelKey new_key{old_order.side, event.price};

  // Will the new level overflow once the old order has left it?
  const Shares leaving = (old_key == new_key) ? old_order.shares : 0;
  if (levels_.total(new_key) - leaving > std::numeric_limits<Shares>::max() - event.shares) {
    return failure(ApplyError::LevelOverflow);
  }

  levels_.sub(old_key, old_order.shares);
  orders_.erase(it);
  orders_.emplace(event.new_order, Order{old_order.side, event.price, event.shares});
  levels_.add(new_key, event.shares);

  ApplyResult result;
  if (old_key == new_key) {
    result.touched[0] = TouchedLevel{new_key, levels_.total(new_key)};
    result.touched_count = 1;
  } else {
    result.touched[0] = TouchedLevel{old_key, levels_.total(old_key)};
    result.touched[1] = TouchedLevel{new_key, levels_.total(new_key)};
    result.touched_count = 2;
  }
  return result;
}

std::optional<InstrumentBook> InstrumentBook::from_orders(std::map<OrderId, Order> orders) {
  InstrumentBook book;
  for (const auto& [id, order] : orders) {
    if (order.shares == 0) return std::nullopt;
    const LevelKey key{order.side, order.price};
    if (!book.levels_.can_add(key, order.shares)) return std::nullopt;
    book.levels_.add(key, order.shares);
  }
  book.orders_ = std::move(orders);
  return book;
}

}  // namespace fh
