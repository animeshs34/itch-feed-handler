#ifndef FH_SIM_CHECKER_HPP_
#define FH_SIM_CHECKER_HPP_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "fh/core.hpp"
#include "fh/nasdaq/moldudp64.hpp"
#include "fh/sim/trace.hpp"

namespace fh::sim {

// The result of replaying one trace through the Nasdaq handler and checking it.
//
// Properties checked here:
//   P1  every update leaves the consumer's books equal to the oracle's        (safety)
//   P2  each Delta's prev equals the last seq published for that instrument   (safety)
//   P3  nothing is published for an instrument while it is recovering         (safety)
//   P9  no BookInconsistent alarm on a valid ground truth                     (safety)
//   P4  at the end, every instrument is live and the books equal the oracle's (liveness)
// P5 (determinism) and P7 (duplicate invariance) compare two runs; see run_twice_equal
// in the campaign.
struct Verdict {
  std::vector<std::string> safety_failures;  // each starts with its property, e.g. "P1: ..."
  std::vector<std::string> liveness_failures;
  std::uint64_t output_digest{0};   // fingerprint of everything the handler published
  std::uint64_t book_digest{0};     // the same, leaving out line-mismatch alarms
  std::uint64_t final_checksum{0};  // checksum of the final state; 0 if there is none
  std::uint64_t updates{0};
  std::uint64_t resets{0};                     // updates that replaced a whole book
  std::array<std::uint64_t, 3> gap_reasons{};  // indexed by GapReason
  std::array<std::uint64_t, 4> feed_health{};  // how often each FeedHealth was entered
  std::array<std::uint64_t, 4> alarms{};       // indexed by AlarmKind
  CoreCounters counters;
  nasdaq::DecoderStats packet_stats;
  nasdaq::DecoderStats snapshot_stats;
  std::vector<std::uint64_t> output_counts;  // how many of each Output alternative were published

  [[nodiscard]] bool safe() const noexcept { return safety_failures.empty(); }
  [[nodiscard]] bool ok() const noexcept {
    return safety_failures.empty() && liveness_failures.empty();
  }
};

// Replays `trace.entries` through a nasdaq::Handler built from `trace.core`, feeds
// every output to a ConsumerModel, and compares the final state with the oracle.
//
// If `outputs` is given, every output the handler published is appended to it, in order.
// Comparing that list with the one a live run collected is how a recorded trace is shown to
// replay exactly.
[[nodiscard]] Verdict run_trace(const Trace& trace, std::vector<Output>* outputs = nullptr);

}  // namespace fh::sim

#endif  // FH_SIM_CHECKER_HPP_
