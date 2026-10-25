#ifndef FH_CORE_HPP_
#define FH_CORE_HPP_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "fh/arbiter.hpp"
#include "fh/instrument_set.hpp"
#include "fh/line_monitor.hpp"
#include "fh/messages.hpp"
#include "fh/outputs.hpp"
#include "fh/sequencer.hpp"
#include "fh/snapshot_reassembler.hpp"
#include "fh/state_codec.hpp"

namespace fh {

struct CoreConfig {
  SequencerConfig sequencer;
  std::size_t arbiter_window{65536};
  LineMonitorConfig lines;
  InstrumentConfig instruments;
};

// Things that happened that are worth counting but do not change the books.
struct CoreCounters {
  std::uint64_t duplicates{0};             // second copies, identical
  std::uint64_t duplicates_unverified{0};  // copies too old to compare
  std::uint64_t line_mismatches{0};        // copies with different bytes
  std::uint64_t late_after_gap{0};         // messages that arrived after we gave up on them
  std::uint64_t covered{0};                // events already inside a loaded snapshot
  std::uint64_t gaps{0};
  std::uint64_t stale_snapshots{0};
  std::uint64_t ignored_snapshots{0};
  std::uint64_t rejected_snapshots{0};
  std::uint64_t inconsistencies{0};
  std::uint64_t parked_overflows{0};

  friend bool operator==(const CoreCounters&, const CoreCounters&) = default;
};

// The whole handler as one deterministic machine: the same inputs in the same
// order always give the same outputs. It reads no clock, opens no socket and
// uses no randomness; time arrives inside the inputs.
//
//   arbiter -> sequencer -> instruments (books, recovery)
//   line monitor (feed health)      snapshot reassembler -> instruments
class Core {
 public:
  explicit Core(const CoreConfig& config);

  // Handles one input and appends everything it produces to `out`.
  void step(const Input& input, std::vector<Output>& out);

  [[nodiscard]] FeedHealth feed_health() const noexcept { return lines_.health(); }
  [[nodiscard]] const InstrumentSet& instruments() const noexcept { return instruments_; }
  [[nodiscard]] const CoreCounters& counters() const noexcept { return counters_; }

  // The next sequence number the core is waiting for, once the feed has been seen.
  [[nodiscard]] std::optional<Seq> expected() const;

  // The books of every instrument seen so far as a FeedState, but only when it
  // is trustworthy: every instrument live and nothing missing in between.
  [[nodiscard]] std::optional<FeedState> export_state() const;

 private:
  void on_packet(const PacketIn& packet, std::vector<Output>& out);
  void on_snapshot_packet(const SnapshotPacketIn& packet, std::vector<Output>& out);
  void on_tick(const TickIn& tick, std::vector<Output>& out);
  void ensure_started(Seq first_expected, std::vector<Output>& out);
  void process(std::vector<SeqEvent>& events, std::vector<Output>& out);

  CoreConfig config_;
  Arbiter arbiter_;
  std::optional<Sequencer> sequencer_;
  LineMonitor lines_;
  SnapshotReassembler reassembler_;
  InstrumentSet instruments_;
  CoreCounters counters_;
};

}  // namespace fh

#endif  // FH_CORE_HPP_
