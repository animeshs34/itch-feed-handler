#include "fh/level_totals.hpp"

#include <cassert>
#include <limits>

namespace fh {

Shares LevelTotals::total(const LevelKey& key) const noexcept {
  const auto it = totals_.find(key);
  return it == totals_.end() ? 0 : it->second;
}

bool LevelTotals::can_add(const LevelKey& key, Shares shares) const noexcept {
  return total(key) <= std::numeric_limits<Shares>::max() - shares;
}

void LevelTotals::add(const LevelKey& key, Shares shares) {
  assert(shares > 0);
  assert(can_add(key, shares));
  totals_[key] += shares;
}

void LevelTotals::sub(const LevelKey& key, Shares shares) {
  assert(shares > 0);
  const auto it = totals_.find(key);
  assert(it != totals_.end() && it->second >= shares);
  it->second -= shares;
  if (it->second == 0) {
    totals_.erase(it);
  }
}

}  // namespace fh
