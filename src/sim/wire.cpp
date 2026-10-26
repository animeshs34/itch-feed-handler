#include "fh/sim/wire.hpp"

#include <algorithm>
#include <span>
#include <utility>

#include "fh/nasdaq/mold_encoder.hpp"

namespace fh::sim {

nasdaq::SessionId main_session() { return nasdaq::make_session("MAINSESS01"); }
nasdaq::SessionId snapshot_session() { return nasdaq::make_session("SNAPSESS01"); }

nasdaq::Bytes message_bytes(const GroundMessage& message) {
  using namespace nasdaq;
  if (!message.raw.empty()) return message.raw;
  if (message.event) {
    EventStyle style;
    style.reduce_as_execute = (message.event->order % 2 == 0);
    style.match_number = message.seq;
    style.timestamp = message.seq * 1000;
    return encode_event(*message.event, style);
  }
  const ItchHeader header{message.locate, 0, message.seq * 1000};
  if (message.filler == 'S') return encode_system_event(header, message.code);
  if (message.filler == 'R')
    return encode_stock_directory(header, synthetic_stock_name(message.locate));
  return encode_filler(message.filler, header);
}

std::vector<Nanos> send_times(std::size_t messages, Nanos spacing, fhtest::SplitMix64& rng,
                              unsigned burst_pct) {
  std::vector<Nanos> out;
  out.reserve(messages);
  Nanos clock = 0;
  std::size_t burst_left = 0;
  for (std::size_t i = 0; i < messages; ++i) {
    if (burst_left == 0 && burst_pct != 0 && rng.below(1000) < burst_pct) {
      burst_left = static_cast<std::size_t>(rng.between(10, 40));
    }
    if (burst_left > 0) {
      --burst_left;
      clock += kMicrosecond;
    } else {
      clock += spacing;
    }
    out.push_back(clock);
  }
  return out;
}

std::vector<Datagram> packetize(const GroundTruth& truth, const std::vector<Nanos>& sent, Line line,
                                fhtest::SplitMix64& rng, unsigned max_pack) {
  std::vector<Datagram> out;
  const std::size_t n = truth.messages.size();
  std::size_t i = 0;
  while (i < n) {
    const std::size_t take = std::min<std::size_t>(
        n - i, static_cast<std::size_t>(rng.between(1, static_cast<std::int64_t>(max_pack))));
    std::vector<nasdaq::Bytes> messages;
    for (std::size_t k = 0; k < take; ++k) messages.push_back(message_bytes(truth.messages[i + k]));
    Datagram d;
    d.sent = sent[i + take - 1];
    d.line = line;
    d.kind = DatagramKind::Messages;
    d.first_seq = truth.messages[i].seq;
    d.count = static_cast<std::uint16_t>(take);
    d.bytes = nasdaq::encode_packet(main_session(), d.first_seq, messages);
    out.push_back(std::move(d));
    i += take;
  }
  return out;
}

Datagram make_heartbeat(Nanos sent, Line line, Seq next_seq) {
  Datagram d;
  d.sent = sent;
  d.line = line;
  d.kind = DatagramKind::Heartbeat;
  d.first_seq = next_seq;
  d.bytes = nasdaq::encode_heartbeat(main_session(), next_seq);
  return d;
}

Datagram make_end_of_session(Nanos sent, Line line, Seq next_seq) {
  Datagram d = make_heartbeat(sent, line, next_seq);
  d.kind = DatagramKind::EndOfSession;
  d.bytes = nasdaq::encode_end_of_session(main_session(), next_seq);
  return d;
}

}  // namespace fh::sim
