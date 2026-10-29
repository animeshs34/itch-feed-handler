#ifndef FH_BENCH_RATE_SWEEP_HPP_
#define FH_BENCH_RATE_SWEEP_HPP_

#include <cstdint>

#include "fh/bench/open_loop.hpp"
#include "fh/bench/workload.hpp"

namespace fh::bench {

// Open loop, handler only: datagram i is due at i / rate. Latency is from the due time to
// the handler having finished with it. Offered at a rate the handler can sustain it shows
// the service time plus a little scheduling noise; above that rate the queue grows and the
// latency with it, which a closed loop would never show.
//
// `stage` says what is timed: the whole handler (bytes in), or the core alone on datagrams
// decoded in advance ("core only").
enum class Stage : std::uint8_t { Handler, Core };

[[nodiscard]] OpenLoopResult bench_open_loop(const Workload& workload, Stage stage,
                                             double datagrams_per_second, std::uint64_t count,
                                             std::uint64_t warmup);

}  // namespace fh::bench

#endif  // FH_BENCH_RATE_SWEEP_HPP_
