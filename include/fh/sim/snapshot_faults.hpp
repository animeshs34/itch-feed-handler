#ifndef FH_SIM_SNAPSHOT_FAULTS_HPP_
#define FH_SIM_SNAPSHOT_FAULTS_HPP_

#include <vector>

#include "fh/sim/delivery.hpp"
#include "fh/sim/ground_truth.hpp"

namespace fh::sim {

// The snapshot loop of a whole day, delivered with its faults.
//
// Every `timing.snapshot_period` the publisher sends one run for the next instrument
// in turn, as of the messages it has sent so far (`sent[n - 1]` is when message n
// goes out). Runs sent before `faulty_until` are exposed to:
//   - the whole run being lost
//   - one datagram in the middle of a run being lost (the rest of the run is useless)
//   - the run arriving so late that it is stale by then
//   - a datagram arriving twice
// Runs are made until `end`. The snapshot stream's own numbering counts lost runs,
// so a lost run shows up as a hole there.
[[nodiscard]] std::vector<Arrival> deliver_snapshots(const GroundTruth& truth,
                                                     const std::vector<Nanos>& sent,
                                                     const Timing& timing,
                                                     const FaultConfig& faults, Nanos faulty_until,
                                                     Nanos end, fhtest::SplitMix64& rng);

}  // namespace fh::sim

#endif  // FH_SIM_SNAPSHOT_FAULTS_HPP_
