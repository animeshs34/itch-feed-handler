#include "fh/core.hpp"

#include <algorithm>
#include <utility>

namespace fh {

Core::Core(const CoreConfig& config)
    : config_(config),
      arbiter_(config.arbiter_window),
      lines_(config.lines),
      instruments_(config.instruments) {}

std::optional<Seq> Core::expected() const {
  if (!sequencer_) return std::nullopt;
  return sequencer_->expected();
}

void Core::step(const Input& input, std::vector<Output>& out) {
  if (const auto* packet = std::get_if<PacketIn>(&input)) {
    on_packet(*packet, out);
  } else if (const auto* snapshot = std::get_if<SnapshotPacketIn>(&input)) {
    on_snapshot_packet(*snapshot, out);
  } else if (const auto* tick = std::get_if<TickIn>(&input)) {
    on_tick(*tick, out);
  }
}

void Core::ensure_started(Seq first_expected, std::vector<Output>& out) {
  if (sequencer_) return;
  const Seq first = std::max<Seq>(first_expected, 1);
  sequencer_.emplace(config_.sequencer, first);
  // Everything before the first sequence number we saw is unknown to us.
  instruments_.set_initial_need(first - 1, out);
}

void Core::on_packet(const PacketIn& packet, std::vector<Output>& out) {
  if (const auto health = lines_.heard(packet.line, packet.t)) {
    out.emplace_back(FeedHealthChange{*health});
  }

  std::vector<SeqEvent> events;
  if (packet.kind == PacketKind::Messages) {
    for (const SequencedMessage& message : packet.messages) {
      ensure_started(message.seq, out);
      switch (arbiter_.observe(message.seq, message.digest)) {
        case Verdict::New:
          sequencer_->accept(message, packet.t, events);
          process(events, out);
          break;
        case Verdict::Duplicate:
          ++counters_.duplicates;
          break;
        case Verdict::DuplicateUnverified:
          ++counters_.duplicates_unverified;
          break;
        case Verdict::Mismatch:
          ++counters_.line_mismatches;
          out.emplace_back(Alarm{AlarmKind::LineMismatch, 0, message.seq});
          break;
      }
    }
  } else {
    ensure_started(packet.next_seq, out);
    sequencer_->note_next_expected(packet.next_seq, packet.t);
    if (packet.kind == PacketKind::EndOfSession) {
      if (const auto health = lines_.end_of_session()) out.emplace_back(FeedHealthChange{*health});
    }
  }

  if (sequencer_) {
    arbiter_.advance(sequencer_->expected());
    counters_.late_after_gap = sequencer_->late_after_gap();
  }
}

void Core::on_tick(const TickIn& tick, std::vector<Output>& out) {
  if (const auto health = lines_.tick(tick.t)) out.emplace_back(FeedHealthChange{*health});
  if (!sequencer_) return;
  std::vector<SeqEvent> events;
  sequencer_->tick(tick.t, events);
  process(events, out);
  arbiter_.advance(sequencer_->expected());
}

void Core::process(std::vector<SeqEvent>& events, std::vector<Output>& out) {
  for (SeqEvent& item : events) {
    if (auto* message = std::get_if<SequencedMessage>(&item)) {
      if (!message->event) continue;  // uses a sequence number, changes no book
      switch (instruments_.on_event(*message->event, out)) {
        case EventOutcome::Covered:
          ++counters_.covered;
          break;
        case EventOutcome::Inconsistent:
          ++counters_.inconsistencies;
          break;
        case EventOutcome::ParkedOverflow:
          ++counters_.parked_overflows;
          break;
        case EventOutcome::Applied:
        case EventOutcome::Parked:
          break;
      }
    } else if (const auto* gap = std::get_if<GapDeclared>(&item)) {
      ++counters_.gaps;
      out.emplace_back(*gap);
      instruments_.on_gap(gap->to, out);
    }
  }
  events.clear();
}

void Core::on_snapshot_packet(const SnapshotPacketIn& packet, std::vector<Output>& out) {
  std::vector<SnapshotRun> runs;
  reassembler_.accept(packet, runs);
  for (const SnapshotRun& run : runs) {
    switch (instruments_.on_snapshot(run, out)) {
      case SnapshotOutcome::Ignored:
        ++counters_.ignored_snapshots;
        break;
      case SnapshotOutcome::Stale:
        ++counters_.stale_snapshots;
        break;
      case SnapshotOutcome::Rejected:
        ++counters_.rejected_snapshots;
        break;
      case SnapshotOutcome::Applied:
        break;
    }
  }
}

std::optional<FeedState> Core::export_state() const {
  if (!sequencer_ || sequencer_->has_hole() || !instruments_.all_live()) return std::nullopt;
  FeedState state;
  state.seq = sequencer_->expected() - 1;
  for (const auto& [id, instrument] : instruments_.states()) {
    state.books.emplace(id, instrument.book());
  }
  return state;
}

}  // namespace fh
