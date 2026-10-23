#include "fh/instrument_state.hpp"

#include <algorithm>
#include <utility>

#include "fh/seeded_bug.hpp"

namespace fh {

InstrumentState::InstrumentState(InstrumentId id, Seq need, const InstrumentConfig& config)
    : id_(id),
      config_(config),
      health_(need == 0 ? InstrumentHealth::Live : InstrumentHealth::Syncing),
      need_(need) {}

EventOutcome InstrumentState::on_event(const BookEvent& event, std::vector<Output>& out) {
  if (event.seq <= last_seq_) return EventOutcome::Covered;
  if (health_ == InstrumentHealth::Live) return apply_live(event, out);

  pending_.emplace(event.seq, event);
  if (pending_.size() > config_.parked_capacity) {
    // Too much to keep. Drop it all and demand a snapshot that covers this event,
    // which covers everything dropped.
    pending_.clear();
    need_ = std::max(need_, event.seq);
    out.emplace_back(Alarm{AlarmKind::ParkedOverflow, id_, event.seq});
    return EventOutcome::ParkedOverflow;
  }
  return EventOutcome::Parked;
}

EventOutcome InstrumentState::apply_live(const BookEvent& event, std::vector<Output>& out) {
  const ApplyResult result = book_.apply(event);
  if (!result.ok()) {
    // The book cannot take a message a valid feed would never send: something
    // was lost without being noticed. Stop trusting the book.
    ++inconsistencies_;
    health_ = InstrumentHealth::Syncing;
    need_ = std::max(need_, event.seq);
    out.emplace_back(Alarm{AlarmKind::BookInconsistent, id_, event.seq});
    out.emplace_back(InstrumentHealthChange{id_, InstrumentHealth::Syncing});
    return EventOutcome::Inconsistent;
  }

  Update update;
  update.kind = UpdateKind::Delta;
  update.inst = id_;
  update.seq = event.seq;
  update.prev = last_seq_;
  update.epoch = epoch_;
  update.event = event;
  update.levels = result.touched;
  update.level_count = result.touched_count;
  out.emplace_back(std::move(update));
  if (!FH_BUG(3)) last_seq_ = event.seq;
  return EventOutcome::Applied;
}

void InstrumentState::on_gap(Seq to, std::vector<Output>& out) {
  need_ = std::max(need_, to);
  if (health_ == InstrumentHealth::Live) {
    health_ = InstrumentHealth::Syncing;
    out.emplace_back(InstrumentHealthChange{id_, InstrumentHealth::Syncing});
  }
}

SnapshotOutcome InstrumentState::on_snapshot(const SnapshotRun& run, std::vector<Output>& out) {
  if (health_ == InstrumentHealth::Live) return SnapshotOutcome::Ignored;
  if (!FH_BUG(1) && run.asof < need_) return SnapshotOutcome::Stale;

  auto book = InstrumentBook::from_orders(run.orders);
  if (!book) {
    out.emplace_back(Alarm{AlarmKind::SnapshotRejected, id_, run.asof});
    return SnapshotOutcome::Rejected;
  }

  book_ = std::move(*book);
  ++epoch_;
  last_seq_ = run.asof;
  health_ = InstrumentHealth::Live;
  out.emplace_back(InstrumentHealthChange{id_, InstrumentHealth::Live});

  Update reset;
  reset.kind = UpdateKind::Reset;
  reset.inst = id_;
  reset.seq = run.asof;
  reset.epoch = epoch_;
  reset.orders = run.orders;
  out.emplace_back(std::move(reset));

  // Events parked while recovering: those the snapshot already reflects are
  // dropped, the rest are replayed in order.
  for (const auto& [seq, event] : pending_) {
    if (!FH_BUG(2) && seq <= run.asof) continue;
    if (apply_live(event, out) == EventOutcome::Inconsistent) break;
  }
  pending_.clear();
  return SnapshotOutcome::Applied;
}

}  // namespace fh
