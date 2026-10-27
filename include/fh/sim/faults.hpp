#ifndef FH_SIM_FAULTS_HPP_
#define FH_SIM_FAULTS_HPP_

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "fh/sim/trace.hpp"
#include "fh/time.hpp"

namespace fh::sim {

// How time runs in a simulated day.
struct Timing {
  Nanos spacing{100 * kMicrosecond};        // between true messages
  Nanos heartbeat_period{kMillisecond};     // per line
  Nanos snapshot_period{4 * kMillisecond};  // one snapshot run per period, instruments in turn
  Nanos tick_period{200 * kMicrosecond};    // how often the handler is told time has passed
  Nanos tail{60 * kMillisecond};            // fault-free time at the end, at least
  std::size_t snapshot_max_messages{8};     // most messages in one snapshot datagram
};

// Every fault the simulator can cause, with how often. All "pct" are percent
// chances per item; "permille" is per thousand. A value of 0 switches that fault
// off. Faults only strike the first `faulty_pct` percent of the day; the rest is
// clean, so a correct handler has the chance to recover and be checked against
// the true books at the end.
struct FaultConfig {
  // --- main feed ---
  unsigned loss_pct{0};             // a datagram is lost on ONE line
  unsigned burst_pct{0};            // a datagram starts a burst that is lost on BOTH lines
  unsigned burst_max{0};            // a burst covers 1..burst_max message intervals of time
  unsigned dup_pct{0};              // a delivered datagram is delivered a second time
  unsigned mismatch_pct{0};         // an altered copy arrives after the genuine one
  unsigned malformed_pct{0};        // a delivered datagram is cut short
  unsigned wrong_session_pct{0};    // an extra datagram from another session arrives
  unsigned silence_windows{0};      // how many stretches where a line, or both, hear nothing
  unsigned end_loss_pct{0};         // the last messages of the day are lost on BOTH lines
  unsigned end_loss_max{0};         // ... 1..end_loss_max message intervals of them
  unsigned rate_burst_permille{0};  // messages squeezed into a burst of sending
  Nanos jitter{0};                  // each delivery is delayed by 0..jitter
  Nanos skew_b{0};                  // line B is this much slower than line A
  // --- snapshot stream ---
  unsigned snap_loss_pct{0};        // a whole run is lost
  unsigned snap_split_loss_pct{0};  // one datagram in the middle of a run is lost
  unsigned snap_late_pct{0};        // a run arrives so late it is stale
  unsigned snap_dup_pct{0};         // a datagram of a run arrives twice
  // --- where ---
  unsigned faulty_pct{0};  // how much of the day is exposed to faults
  CoreParams core;
};

enum class Tier : std::uint8_t { Clean, Mild, Moderate, Heavy, Brutal };
inline constexpr unsigned kTierCount = 5;

[[nodiscard]] FaultConfig tier_config(Tier tier);
[[nodiscard]] std::string_view tier_name(Tier tier);

}  // namespace fh::sim

#endif  // FH_SIM_FAULTS_HPP_
