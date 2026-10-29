#ifndef FH_BENCH_RECOVERY_HPP_
#define FH_BENCH_RECOVERY_HPP_

#include <cstdint>

#include "fh/bench/histogram.hpp"
#include "fh/sim/faults.hpp"
#include "fh/sim/scenario.hpp"

namespace fh::bench {

// How long an instrument stays in recovery: from the moment it stops being trusted (a gap
// was declared, or its book proved inconsistent) to the moment a snapshot makes it live
// again.
//
// This is measured in SIMULATED time, the timestamps of the trace's entries, not in CPU
// time. It depends on the snapshot loop period, the number of instruments and the faults,
// not on how fast the machine is, so it is deterministic: the same seeds give the same
// numbers on every machine. It is reported apart from steady-state latency and never mixed
// with it.
struct RecoveryResult {
  LogLinearHistogram recovery;  // nanoseconds of simulated time, one sample per episode
  std::uint64_t scenarios{0};
  std::uint64_t episodes{0};     // times an instrument went into recovery and came out
  std::uint64_t unrecovered{0};  // instruments still in recovery when the trace ended
  Nanos loop_period{0};          // one full cycle of the snapshot loop (period x instruments)
};

// Runs `scenarios` simulated days of `tier`, seeds first_seed, first_seed + 1, ..., through the
// Nasdaq handler and measures every recovery episode.
[[nodiscard]] RecoveryResult bench_recovery(sim::Tier tier, std::uint64_t first_seed,
                                            std::uint64_t scenarios);

}  // namespace fh::bench

#endif  // FH_BENCH_RECOVERY_HPP_
