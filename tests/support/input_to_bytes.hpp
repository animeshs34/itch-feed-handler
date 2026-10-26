#ifndef FH_TESTS_SUPPORT_INPUT_TO_BYTES_HPP_
#define FH_TESTS_SUPPORT_INPUT_TO_BYTES_HPP_

#include <variant>
#include <vector>

#include "byte_truth.hpp"
#include "fh/messages.hpp"

namespace fhtest {

// Turns a decoded input back into the datagram that would have produced it. This
// lets the same random scenario run twice: once straight into the core, once as
// bytes through the Nasdaq handler. The two must agree exactly.
inline ByteStep to_byte_step(const fh::Input& input, const fh::nasdaq::SessionId& main_session,
                             const fh::nasdaq::SessionId& snapshot_session) {
  using namespace fh::nasdaq;
  if (const auto* packet = std::get_if<fh::PacketIn>(&input)) {
    ByteStep step{ByteStep::Kind::Main, packet->t, packet->line, {}};
    if (packet->kind == fh::PacketKind::Heartbeat) {
      step.bytes = encode_heartbeat(main_session, packet->next_seq);
    } else if (packet->kind == fh::PacketKind::EndOfSession) {
      step.bytes = encode_end_of_session(main_session, packet->next_seq);
    } else {
      std::vector<Bytes> messages;
      for (const fh::SequencedMessage& m : packet->messages) {
        if (m.event) {
          EventStyle style;
          style.reduce_as_execute = (m.event->order % 2 == 0);
          style.match_number = m.seq;
          style.timestamp = m.seq * 1000;
          messages.push_back(encode_event(*m.event, style));
        } else {
          messages.push_back(encode_filler('N', ItchHeader{}));
        }
      }
      const fh::Seq first = packet->messages.empty() ? 1 : packet->messages.front().seq;
      step.bytes = encode_packet(main_session, first, messages);
    }
    return step;
  }
  if (const auto* snap = std::get_if<fh::SnapshotPacketIn>(&input)) {
    ByteStep step{ByteStep::Kind::Snapshot, snap->t, fh::Line::A, {}};
    if (snap->items.empty()) {
      step.bytes = encode_heartbeat(snapshot_session, snap->stream_seq);
      return step;
    }
    std::vector<Bytes> messages;
    for (const fh::SnapshotItem& item : snap->items) {
      if (const auto* open = std::get_if<fh::SnapshotOpen>(&item)) {
        messages.push_back(
            encode_stock_directory(ItchHeader{open->inst, 0, 0}, synthetic_stock_name(open->inst)));
      } else if (const auto* order = std::get_if<fh::SnapshotOrder>(&item)) {
        messages.push_back(encode_add(ItchHeader{order->inst, 0, 0}, order->id, order->order.side,
                                      order->order.shares, synthetic_stock_name(order->inst),
                                      order->order.price));
      } else if (const auto* close = std::get_if<fh::SnapshotClose>(&item)) {
        messages.push_back(encode_end_of_snapshot(close->asof + 1));
      }
    }
    step.bytes = encode_packet(snapshot_session, snap->stream_seq, messages);
    return step;
  }
  const auto& tick = std::get<fh::TickIn>(input);
  return ByteStep{ByteStep::Kind::Tick, tick.t, fh::Line::A, {}};
}

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_INPUT_TO_BYTES_HPP_
