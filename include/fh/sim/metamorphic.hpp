#ifndef FH_SIM_METAMORPHIC_HPP_
#define FH_SIM_METAMORPHIC_HPP_

#include <cstdint>
#include <optional>
#include <string>

#include "fh/sim/trace.hpp"

namespace fh::sim {

// Properties that compare two runs instead of checking one against the oracle.

// P7, duplicate invariance. Returns a copy of `trace` in which each main-feed entry is
// followed, with chance `pct` percent, by an identical copy at the same instant. A
// handler that takes the first copy and recognises the second must publish exactly
// what it published before: same updates, same health changes, same gaps. Only
// counters (the duplicate count) and line-mismatch alarms may differ, because copying a
// damaged datagram is one more damaged copy for the handler to report.
//
// Snapshot-stream entries are not duplicated here: the snapshot stream has its own
// rules for repeats, which the snapshot faults already exercise.
[[nodiscard]] Trace with_duplicates(const Trace& trace, std::uint64_t seed, unsigned pct);

// Runs the trace and its duplicated twin and compares what was published. Returns a
// description of the difference, or nothing if they agree (P7).
[[nodiscard]] std::optional<std::string> check_duplicate_invariance(const Trace& trace,
                                                                    std::uint64_t seed);

// P5, determinism. Runs the same trace twice, with two separate handlers, and compares
// the fingerprint of every output, the final checksum and the counters.
[[nodiscard]] std::optional<std::string> check_determinism(const Trace& trace);

}  // namespace fh::sim

#endif  // FH_SIM_METAMORPHIC_HPP_
