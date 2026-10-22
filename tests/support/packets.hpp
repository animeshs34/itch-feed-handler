#ifndef FH_TESTS_SUPPORT_PACKETS_HPP_
#define FH_TESTS_SUPPORT_PACKETS_HPP_

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "fh/checksum.hpp"
#include "fh/messages.hpp"

namespace fhtest {

// A fingerprint of an event's fields, standing in for the fingerprint of the
// original bytes that the real decoder will provide.
inline std::uint64_t digest_of(const fh::BookEvent& e) {
  std::vector<std::uint8_t> bytes;
  const auto put = [&bytes](std::uint64_t v) {
    for (int i = 0; i < 8; ++i) bytes.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
  };
  put(static_cast<std::uint64_t>(e.kind));
  put(e.seq);
  put(e.inst);
  put(e.order);
  put(e.new_order);
  put(static_cast<std::uint64_t>(e.side));
  put(static_cast<std::uint64_t>(e.price));
  put(e.shares);
  return fh::fnv1a64(bytes);
}

inline fh::SequencedMessage message(const fh::BookEvent& e) {
  return fh::SequencedMessage{e.seq, e, digest_of(e)};
}

// A message that uses a sequence number but changes no book.
inline fh::SequencedMessage no_effect(fh::Seq seq, std::uint64_t digest = 0) {
  return fh::SequencedMessage{seq, std::nullopt, digest};
}

inline fh::PacketIn packet(fh::Line line, fh::Nanos t, std::vector<fh::SequencedMessage> messages) {
  fh::PacketIn p;
  p.t = t;
  p.line = line;
  p.kind = fh::PacketKind::Messages;
  p.messages = std::move(messages);
  return p;
}

inline fh::PacketIn heartbeat(fh::Line line, fh::Nanos t, fh::Seq next_seq) {
  fh::PacketIn p;
  p.t = t;
  p.line = line;
  p.kind = fh::PacketKind::Heartbeat;
  p.next_seq = next_seq;
  return p;
}

inline fh::PacketIn end_of_session(fh::Line line, fh::Nanos t, fh::Seq next_seq) {
  fh::PacketIn p = heartbeat(line, t, next_seq);
  p.kind = fh::PacketKind::EndOfSession;
  return p;
}

inline fh::TickIn tick(fh::Nanos t) { return fh::TickIn{t}; }

// The items of one snapshot run: Open, one Order per live order, Close.
inline std::vector<fh::SnapshotItem> run_items(fh::InstrumentId inst, fh::Seq asof,
                                               const std::map<fh::OrderId, fh::Order>& orders) {
  std::vector<fh::SnapshotItem> items;
  items.emplace_back(fh::SnapshotOpen{inst});
  for (const auto& [id, order] : orders) items.emplace_back(fh::SnapshotOrder{inst, id, order});
  items.emplace_back(fh::SnapshotClose{asof});
  return items;
}

inline fh::SnapshotPacketIn snapshot_packet(fh::Nanos t, fh::Seq stream_seq,
                                            std::vector<fh::SnapshotItem> items) {
  fh::SnapshotPacketIn p;
  p.t = t;
  p.stream_seq = stream_seq;
  p.items = std::move(items);
  return p;
}

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_PACKETS_HPP_
