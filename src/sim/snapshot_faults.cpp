#include "fh/sim/snapshot_faults.hpp"

#include <algorithm>
#include <iterator>

#include "fh/sim/snapshots.hpp"

namespace fh::sim {
namespace {

// How late a "late" run is: longer than the stale limit of the snapshot rules.
constexpr Nanos kLateBy = 6 * kMillisecond;
constexpr Nanos kMaxTransit = 3 * kMillisecond;

}  // namespace

std::vector<Arrival> deliver_snapshots(const GroundTruth& truth, const std::vector<Nanos>& sent,
                                       const Timing& timing, const FaultConfig& faults,
                                       Nanos faulty_until, Nanos end, fhtest::SplitMix64& rng) {
  std::vector<Arrival> out;
  const std::set<InstrumentId> inst_set = truth.instruments();
  const std::vector<InstrumentId> insts(inst_set.begin(), inst_set.end());
  if (insts.empty()) return out;

  SnapshotBuilder builder(truth);
  std::size_t turn = 0;
  for (Nanos t = timing.snapshot_period; t <= end; t += timing.snapshot_period, ++turn) {
    const InstrumentId inst = insts[turn % insts.size()];
    const Seq asof = static_cast<Seq>(std::upper_bound(sent.begin(), sent.end(), t) - sent.begin());
    auto run = builder.build_run(t, inst, asof, timing.snapshot_max_messages);

    const bool exposed = t < faulty_until;
    if (exposed && rng.below(100) < faults.snap_loss_pct) continue;
    if (exposed && run.size() >= 2 && rng.below(100) < faults.snap_split_loss_pct) {
      run.erase(run.begin() + static_cast<std::ptrdiff_t>(rng.below(run.size())));
    }
    Nanos base = t + rng.between(0, kMaxTransit);
    if (exposed && rng.below(100) < faults.snap_late_pct) base += kLateBy;

    for (std::size_t i = 0; i < run.size(); ++i) {
      Arrival a;
      a.entry.t = base + static_cast<Nanos>(run[i].index) * 10 * kMicrosecond;
      a.entry.kind = EntryKind::Snapshot;
      a.entry.bytes = run[i].bytes;
      a.first_seq = run[i].first_stream_seq;
      a.count = run[i].count;
      out.push_back(a);
      if (exposed && rng.below(100) < faults.snap_dup_pct) {
        a.entry.t += kMicrosecond + rng.between(0, kMaxTransit);
        out.push_back(a);
      }
    }
  }
  return out;
}

}  // namespace fh::sim
