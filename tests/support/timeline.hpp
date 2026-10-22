#ifndef FH_TESTS_SUPPORT_TIMELINE_HPP_
#define FH_TESTS_SUPPORT_TIMELINE_HPP_

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

#include "fh/messages.hpp"
#include "oracle_state.hpp"
#include "packets.hpp"

namespace fhtest {

// Builds a list of inputs at chosen times, the way the walkthrough scenarios are
// written: "line A delivers seq 6 at t=6". One unit of time is one millisecond.
// A clock tick is added at every whole unit, after anything else that happens at
// that unit.
class Timeline {
 public:
  static constexpr fh::Nanos kUnit = fh::kMillisecond;

  explicit Timeline(const std::vector<fh::BookEvent>& truth) : truth_(truth) {}

  void deliver(fh::Line line, fh::Nanos unit, fh::Seq seq) {
    add(unit, fh::Input{packet(line, unit * kUnit, {message(event(seq))})});
  }

  // Delivers a copy whose bytes differ from the real one.
  void deliver_altered(fh::Line line, fh::Nanos unit, fh::Seq seq) {
    fh::SequencedMessage m = message(event(seq));
    m.digest ^= 0xFFFF;
    add(unit, fh::Input{packet(line, unit * kUnit, {m})});
  }

  void heartbeat(fh::Line line, fh::Nanos unit, fh::Seq next_seq) {
    add(unit, fh::Input{fhtest::heartbeat(line, unit * kUnit, next_seq)});
  }

  void end_of_session(fh::Line line, fh::Nanos unit, fh::Seq next_seq) {
    add(unit, fh::Input{fhtest::end_of_session(line, unit * kUnit, next_seq)});
  }

  // A complete snapshot run for one instrument, built by the oracle.
  void snapshot(fh::Nanos unit, fh::InstrumentId inst, fh::Seq asof) {
    auto items = run_items(inst, asof, oracle_orders(truth_, inst, asof));
    const fh::Seq first = stream_seq_;
    stream_seq_ += items.size();
    add(unit, fh::Input{snapshot_packet(unit * kUnit, first, std::move(items))});
  }

  // All inputs in time order, with a tick at every unit up to `end_unit`.
  std::vector<fh::Input> build(fh::Nanos end_unit) const {
    std::vector<fh::Input> out;
    std::size_t next = 0;
    for (fh::Nanos unit = 1; unit <= end_unit; ++unit) {
      while (next < items_.size() && items_[next].first == unit)
        out.push_back(items_[next++].second);
      out.emplace_back(tick(unit * kUnit));
    }
    return out;
  }

  // Delivers `seqs` on `line` at unit = seq + offset.
  void deliver_all(fh::Line line, const std::vector<fh::Seq>& seqs, fh::Nanos offset) {
    for (const fh::Seq s : seqs) deliver(line, static_cast<fh::Nanos>(s) + offset, s);
  }

 private:
  const fh::BookEvent& event(fh::Seq seq) const { return truth_[seq - 1]; }

  void add(fh::Nanos unit, fh::Input input) {
    // Keep items sorted by unit; items at the same unit keep the order they were added.
    const auto pos = std::upper_bound(items_.begin(), items_.end(), unit,
                                      [](fh::Nanos u, const auto& item) { return u < item.first; });
    items_.insert(pos, {unit, std::move(input)});
  }

  const std::vector<fh::BookEvent>& truth_;
  std::vector<std::pair<fh::Nanos, fh::Input>> items_;
  fh::Seq stream_seq_{1};
};

inline std::vector<fh::Seq> seqs(fh::Seq from, fh::Seq to, const std::vector<fh::Seq>& skip = {}) {
  std::vector<fh::Seq> out;
  for (fh::Seq s = from; s <= to; ++s) {
    if (std::find(skip.begin(), skip.end(), s) == skip.end()) out.push_back(s);
  }
  return out;
}

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_TIMELINE_HPP_
