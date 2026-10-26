#ifndef FH_SIM_DAY_HPP_
#define FH_SIM_DAY_HPP_

#include <cstddef>
#include <cstdint>

#include "fh/sim/ground_truth.hpp"

namespace fh::sim {

struct DayConfig {
  std::uint16_t instruments{4};
  std::size_t book_events{400};  // how many messages change a book
  unsigned filler_every{6};      // a trade-like filler after every this-many book events; 0 = none
};

// A synthetic trading day, from one seed, in the order a real Nasdaq feed tells it:
//
//   S/O  start of messages
//   R    stock directory, one per instrument
//   H    trading action, one per instrument
//   Y    reg SHO indicator, one per instrument
//   S/S  start of system hours
//   S/Q  start of market hours
//   ...  the book events, with a trade-like filler (P, I, B or N) every few of them
//   S/M  end of market hours
//   S/E  end of system hours
//   S/C  end of messages
//
// The book events are valid by construction (a reduce never removes more shares
// than an order has, a delete names a live order). The same seed and config always
// give the same day, on every compiler.
[[nodiscard]] GroundTruth make_day(std::uint64_t seed, const DayConfig& config);

}  // namespace fh::sim

#endif  // FH_SIM_DAY_HPP_
