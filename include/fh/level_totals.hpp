#ifndef FH_LEVEL_TOTALS_HPP_
#define FH_LEVEL_TOTALS_HPP_

#include <map>

#include "fh/types.hpp"

namespace fh {

// Identifies one price level of one side of a book.
struct LevelKey {
  Side side{Side::Buy};
  Price price{0};

  friend constexpr auto operator<=>(const LevelKey&, const LevelKey&) = default;
};

// Running total of shares per price level (the L2 view).
//
// Invariant: no entry has a total of zero, and every total fits in Shares.
// A level that drops to zero is erased, so "no level" and "zero shares" are
// the same state and cannot disagree.
class LevelTotals {
 public:
  // Total at `key`, or 0 if there is no such level.
  [[nodiscard]] Shares total(const LevelKey& key) const noexcept;

  // True if adding `shares` to `key` would not overflow.
  [[nodiscard]] bool can_add(const LevelKey& key, Shares shares) const noexcept;

  // Precondition: can_add(key, shares) and shares > 0.
  void add(const LevelKey& key, Shares shares);

  // Precondition: total(key) >= shares and shares > 0.
  void sub(const LevelKey& key, Shares shares);

  // Every level, sorted by (side, price).
  [[nodiscard]] const std::map<LevelKey, Shares>& all() const noexcept { return totals_; }

 private:
  std::map<LevelKey, Shares> totals_;
};

}  // namespace fh

#endif  // FH_LEVEL_TOTALS_HPP_
