#ifndef FH_SIM_CONSISTENCY_HPP_
#define FH_SIM_CONSISTENCY_HPP_

#include <cstdint>
#include <vector>

#include "fh/events.hpp"
#include "fh/sim/ground_truth.hpp"

namespace fh::sim {

// One message that a valid stream would never contain.
struct Violation {
  Seq seq{0};
  EventKind kind{EventKind::Add};
  InstrumentId inst{0};
  OrderId order{0};

  friend bool operator==(const Violation&, const Violation&) = default;
};

struct ConsistencyReport {
  std::uint64_t events_checked{0};
  std::uint64_t violations{0};
  std::vector<Violation> first;  // the first few, in order (at most kMaxListed)
  bool numbering_ok{true};       // messages[i].seq == i + 1 everywhere

  [[nodiscard]] bool ok() const noexcept { return violations == 0 && numbering_ok; }
};

inline constexpr std::size_t kMaxListedViolations = 20;

// Checks the assumption the whole handler rests on: a valid
// ITCH stream is internally consistent. Every execute, cancel, delete and replace names a
// live order, a reduce never removes more shares than the order has, and an add never
// reuses a live order reference.
//
// A violation in a recorded file means the file is not a valid ground truth (for example
// it was sliced from the middle of the day). It does not use the handler's book, so a bug
// in the book cannot hide a violation. After a violation the check goes on, leaving the
// offending message out of its model.
[[nodiscard]] ConsistencyReport check_consistency(const GroundTruth& truth);

}  // namespace fh::sim

#endif  // FH_SIM_CONSISTENCY_HPP_
