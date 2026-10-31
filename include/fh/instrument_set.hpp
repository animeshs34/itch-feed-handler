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

  // The index points into this object's own map, so a copy has to build its own.
  // A move keeps the map's nodes where they are, so the pointers stay valid.
  InstrumentSet(const InstrumentSet& other)
      : config_(other.config_), global_need_(other.global_need_), states_(other.states_) {
    rebuild_index();
  }
  InstrumentSet& operator=(const InstrumentSet& other) {
    if (this != &other) {
      config_ = other.config_;
      global_need_ = other.global_need_;
      states_ = other.states_;
      rebuild_index();
    }
    return *this;
  }
  InstrumentSet(InstrumentSet&&) noexcept = default;
  InstrumentSet& operator=(InstrumentSet&&) noexcept = default;

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
  void rebuild_index() {
    index_.assign(65536, nullptr);
    for (auto& [id, state] : states_) index_[id] = &state;
  }

  InstrumentConfig config_;
  Seq global_need_{0};
  // The map owns the states and keeps them in id order (gap handling and the state
  // encoding rely on that order). Its nodes never move, so the table below can point at
  // them: one array index per message instead of a tree search.
  std::map<InstrumentId, InstrumentState> states_;
  std::vector<InstrumentState*> index_ = std::vector<InstrumentState*>(65536, nullptr);
};

}  // namespace fh

#endif  // FH_INSTRUMENT_SET_HPP_
