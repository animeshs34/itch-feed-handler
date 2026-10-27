#ifndef FH_SIM_SHRINK_HPP_
#define FH_SIM_SHRINK_HPP_

#include <cstddef>
#include <functional>

#include "fh/sim/trace.hpp"

namespace fh::sim {

// Makes a failing trace smaller while it keeps failing, so that a person can read it.
//
// It removes entries (datagrams and ticks), never changes one, and never touches the
// ground truth. It uses delta debugging (Zeller and Hildebrandt, 2002): remove ever
// smaller chunks, keeping a removal whenever `still_fails` says the trace still fails,
// until no single entry can be removed. With enough budget the result is 1-minimal.
//
// `max_runs` bounds how many times `still_fails` is called; when it runs out the best
// trace so far is returned. The result is deterministic.
[[nodiscard]] Trace shrink(const Trace& trace, const std::function<bool(const Trace&)>& still_fails,
                           std::size_t max_runs = 3000);

// Shrinks a trace that breaks a SAFETY property (P1, P2, P3, P9), by replaying it.
//
// Liveness failures (P4) are not shrunk: removing entries trivially breaks liveness (a
// handler cannot recover from snapshots that were deleted), so "still fails" would stay
// true for the wrong reason and the shrunk trace would show nothing about the real bug.
[[nodiscard]] Trace shrink_safety_failure(const Trace& trace, std::size_t max_runs = 3000);

}  // namespace fh::sim

#endif  // FH_SIM_SHRINK_HPP_
