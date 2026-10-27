#ifndef FH_SIM_SCENARIO_HPP_
#define FH_SIM_SCENARIO_HPP_

#include <cstdint>

#include "fh/sim/day.hpp"
#include "fh/sim/faults.hpp"
#include "fh/sim/trace.hpp"

namespace fh::sim {

// Everything that shapes one simulated day.
struct ScenarioConfig {
  DayConfig day;
  Timing timing;
  FaultConfig faults;
};

// The usual day with the faults of one tier.
[[nodiscard]] ScenarioConfig scenario_config(Tier tier);

// Builds a complete trace from one seed: the true day, both lines of the main feed
// with every fault applied, the snapshot loop with its faults, the heartbeats, the end
// of session, and a tick for every `tick_period`, all in arrival order.
//
// The same seed and config always give the same trace, byte for byte, on every
// compiler. Each kind of fault draws from its own random stream (all derived from the
// seed), so changing one fault's setting does not reshuffle the others.
[[nodiscard]] Trace make_trace(std::uint64_t seed, const ScenarioConfig& config);

// The same, for a ground truth made elsewhere (a recorded file, for example). The seed
// drives every fault; `config.day` is ignored. For a synthetic truth,
// make_trace(seed, c) == make_trace_for(make_day(...), seed, c) by construction. An empty
// ground truth gives an empty trace.
[[nodiscard]] Trace make_trace_for(const GroundTruth& truth, std::uint64_t seed,
                                   const ScenarioConfig& config);

}  // namespace fh::sim

#endif  // FH_SIM_SCENARIO_HPP_
