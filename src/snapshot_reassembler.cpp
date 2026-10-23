#include "fh/snapshot_reassembler.hpp"

#include <utility>

namespace fh {

void SnapshotReassembler::discard_partial() {
  if (partial_) ++discarded_runs_;
  partial_.reset();
}

void SnapshotReassembler::accept(const SnapshotPacketIn& packet,
                                 std::vector<SnapshotRun>& completed) {
  // A break in the snapshot stream's own numbering means a piece was lost.
  if (next_stream_seq_ && packet.stream_seq != *next_stream_seq_) discard_partial();
  next_stream_seq_ = packet.stream_seq + packet.items.size();

  for (const SnapshotItem& item : packet.items) {
    if (const auto* open = std::get_if<SnapshotOpen>(&item)) {
      discard_partial();  // an Open while a run is still open: the old run was cut short
      partial_ = SnapshotRun{open->inst, 0, {}};
    } else if (const auto* order = std::get_if<SnapshotOrder>(&item)) {
      if (!partial_) continue;  // the Open was missed; wait for the next run
      if (order->inst != partial_->inst ||
          !partial_->orders.emplace(order->id, order->order).second) {
        discard_partial();  // wrong instrument or a repeated order id: not trustworthy
      }
    } else if (const auto* close = std::get_if<SnapshotClose>(&item)) {
      if (!partial_) continue;
      partial_->asof = close->asof;
      completed.push_back(std::move(*partial_));
      partial_.reset();
    }
  }
}

}  // namespace fh
