#ifndef FH_SIM_TRACE_HPP_
#define FH_SIM_TRACE_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "fh/core.hpp"
#include "fh/messages.hpp"
#include "fh/nasdaq/itch_encoder.hpp"
#include "fh/sim/ground_truth.hpp"
#include "fh/time.hpp"

namespace fh::sim {

// What reaches the handler, one thing at a time, in the order it arrives.
enum class EntryKind : std::uint8_t {
  Main,      // a datagram from line `line` of the main feed
  Snapshot,  // a datagram from the snapshot stream
  Tick,      // time passing with nothing received
};

struct Entry {
  Nanos t{0};
  EntryKind kind{EntryKind::Tick};
  Line line{Line::A};   // Main only
  nasdaq::Bytes bytes;  // Main and Snapshot only

  friend bool operator==(const Entry&, const Entry&) = default;
};

// The few core settings a run depends on. They travel with the trace so that a
// trace file replays exactly the way it was recorded.
struct CoreParams {
  Nanos skew_window{kMillisecond};
  Nanos silent_window{6 * kMillisecond};
  std::size_t buffer_capacity{65536};
  std::size_t arbiter_window{65536};
  std::size_t parked_capacity{65536};

  friend bool operator==(const CoreParams&, const CoreParams&) = default;
};

[[nodiscard]] CoreConfig to_core_config(const CoreParams& params);

// A complete run: the true feed, the core settings, and everything the handler is
// shown. Replaying `entries` through a handler built from `core` must leave every
// book equal to the one the true feed implies.
struct Trace {
  std::uint64_t seed{0};  // informational: which seed made this trace
  GroundTruth truth;
  CoreParams core;
  std::vector<Entry> entries;

  friend bool operator==(const Trace&, const Trace&) = default;
};

}  // namespace fh::sim

#endif  // FH_SIM_TRACE_HPP_
