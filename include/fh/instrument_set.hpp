#ifndef FH_INSTRUMENT_SET_HPP_
#define FH_INSTRUMENT_SET_HPP_

#include <map>
#include <vector>

#include "fh/instrument_state.hpp"
#include "fh/outputs.hpp"
#include "fh/snapshot_reassembler.hpp"
#include "fh/types.hpp"

namespace fh {

// All the instruments the handler knows about.
//
// A lost message could have belonged to any instrument, so a gap affects every
// one of them, including instruments first seen later. That is what
// `global_need` remembers: the end of the latest gap (or of everything missed
// before we joined). An instrument created while it is above zero starts out
// recovering.
class InstrumentSet {
 public:
  explicit InstrumentSet(const InstrumentConfig& config) : config_(config) {}

  // Called once, when the feed is first seen: everything before the first
  // sequence number we saw is unknown. An instrument that already exists (a snapshot
  // can arrive before the first main-feed packet) is live only on the strength of an
  // empty book, so it must start recovering like every instrument created later.
  void set_initial_need(Seq need, std::vector<Output>& out);

  EventOutcome on_event(const BookEvent& event, std::vector<Output>& out);
  void on_gap(Seq to, std::vector<Output>& out);
  SnapshotOutcome on_snapshot(const SnapshotRun& run, std::vector<Output>& out);

  [[nodiscard]] const std::map<InstrumentId, InstrumentState>& states() const noexcept {
    return states_;
  }
  [[nodiscard]] Seq global_need() const noexcept { return global_need_; }
  [[nodiscard]] bool all_live() const noexcept;

 private:
  InstrumentState& get_or_create(InstrumentId id, std::vector<Output>& out);

  InstrumentConfig config_;
  Seq global_need_{0};
  std::map<InstrumentId, InstrumentState> states_;
};

}  // namespace fh

#endif  // FH_INSTRUMENT_SET_HPP_
