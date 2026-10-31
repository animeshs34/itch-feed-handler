#include "fh/level_totals.hpp"

#include <cassert>
#include <limits>

namespace fh {

Shares LevelTotals::total(const LevelKey& key) const noexcept {
  const Shares* found = totals_.find(key);
  return found ? *found : 0;
}

bool LevelTotals::can_add(const LevelKey& key, Shares shares) const noexcept {
  return total(key) <= std::numeric_limits<Shares>::max() - shares;
}

void LevelTotals::add(const LevelKey& key, Shares shares) {
  assert(shares > 0);
  assert(can_add(key, shares));
  totals_.get_or_insert(key, 0) += shares;
}

void LevelTotals::sub(const LevelKey& key, Shares shares) {
  assert(shares > 0);
  Shares* found = totals_.find(key);
  assert(found != nullptr && *found >= shares);
  *found -= shares;
  if (*found == 0) {
    totals_.erase(key);
  }
}

std::map<LevelKey, Shares> LevelTotals::all() const {
  std::map<LevelKey, Shares> sorted;
  totals_.for_each([&](const LevelKey& key, Shares total) { sorted.emplace(key, total); });
  return sorted;
}

}  // namespace fh
