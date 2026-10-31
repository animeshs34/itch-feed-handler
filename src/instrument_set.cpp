#include "fh/instrument_set.hpp"

#include <algorithm>

namespace fh {

InstrumentState& InstrumentSet::get_or_create(InstrumentId id, std::vector<Output>& out) {
  if (InstrumentState* known = index_[id]) return *known;
  const auto it = states_.emplace(id, InstrumentState(id, global_need_, config_)).first;
  index_[id] = &it->second;
  out.emplace_back(InstrumentHealthChange{id, it->second.health()});
  return it->second;
}

void InstrumentSet::set_initial_need(Seq need, std::vector<Output>& out) {
  global_need_ = need;
  if (need == 0) return;  // the feed starts at 1: nothing is missing
  for (auto& [id, state] : states_) state.on_gap(need, out);
}

EventOutcome InstrumentSet::on_event(const BookEvent& event, std::vector<Output>& out) {
  return get_or_create(event.inst, out).on_event(event, out);
}

void InstrumentSet::on_gap(Seq to, std::vector<Output>& out) {
  global_need_ = std::max(global_need_, to);
  for (auto& [id, state] : states_) state.on_gap(to, out);
}

SnapshotOutcome InstrumentSet::on_snapshot(const SnapshotRun& run, std::vector<Output>& out) {
  return get_or_create(run.inst, out).on_snapshot(run, out);
}

bool InstrumentSet::all_live() const noexcept {
  return std::all_of(states_.begin(), states_.end(), [](const auto& entry) {
    return entry.second.health() == InstrumentHealth::Live;
  });
}

}  // namespace fh
