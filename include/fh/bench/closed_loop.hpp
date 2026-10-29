#ifndef FH_BENCH_CLOSED_LOOP_HPP_
#define FH_BENCH_CLOSED_LOOP_HPP_

#include <cstdint>

#include "fh/bench/histogram.hpp"
#include "fh/bench/workload.hpp"

namespace fh::bench {

// Closed-loop timing: the next datagram is handled when the last one is done. This is a
// measure of the *service time* of the code (how long one call takes), not of latency under
// load; the open-loop benchmarks measure that.
struct ClosedLoopResult {
  LogLinearHistogram service;     // time per datagram, one timed call each
  double messages_per_second{0};  // from a separate pass with no per-datagram timing
  double datagrams_per_second{0};
  std::uint64_t messages{0};
  std::uint64_t outputs{0};  // everything the handler published in the untimed pass
};

// The whole Nasdaq handler: bytes in, updates out. `repeats` timed passes, each with a fresh
// handler, then one untimed pass for the throughput.
[[nodiscard]] ClosedLoopResult bench_handler_closed_loop(const Workload& workload,
                                                         unsigned repeats);

// The core alone, on datagrams decoded in advance: what the decoding adds is the difference
// between the two.
[[nodiscard]] ClosedLoopResult bench_core_closed_loop(const Workload& workload, unsigned repeats);

}  // namespace fh::bench

#endif  // FH_BENCH_CLOSED_LOOP_HPP_
