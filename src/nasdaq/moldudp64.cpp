#include "fh/nasdaq/moldudp64.hpp"

#include <algorithm>
#include <limits>

#include "fh/checksum.hpp"
#include "fh/nasdaq/be_reader.hpp"

namespace fh::nasdaq {

ParsedPacket parse_packet(std::span<const std::uint8_t> bytes) {
  ParsedPacket parsed;
  BeReader in(bytes);

  const auto session = in.take_bytes(10);
  const auto seq = in.u64();
  const auto count = in.u16();
  if (!session || !seq || !count) {
    parsed.error = PacketError::TooShort;
    return parsed;
  }
  std::copy(session->begin(), session->end(), parsed.header.session.begin());
  parsed.header.seq = *seq;
  parsed.header.count = *count;

  if (*count == kMoldHeartbeatCount || *count == kMoldEndOfSessionCount) {
    if (in.remaining() != 0) parsed.error = PacketError::HeartbeatHasBody;
    return parsed;
  }

  // The numbers of the messages are seq, seq + 1, ... seq + count - 1.
  if (*seq > std::numeric_limits<fh::Seq>::max() - *count) {
    parsed.error = PacketError::BadSequence;
    return parsed;
  }

  parsed.blocks.reserve(*count);
  for (std::uint16_t i = 0; i < *count; ++i) {
    const auto length = in.u16();
    const auto block = length ? in.take_bytes(*length) : std::nullopt;
    if (!block) {
      parsed.error = PacketError::BlockOverrun;
      parsed.blocks.clear();
      return parsed;
    }
    parsed.blocks.push_back(*block);
  }
  if (in.remaining() != 0) {
    parsed.error = PacketError::TrailingBytes;
    parsed.blocks.clear();
  }
  return parsed;
}

std::optional<fh::PacketIn> PacketDecoder::decode(fh::Nanos t, fh::Line line,
                                                  std::span<const std::uint8_t> bytes) {
  ++stats_.packets;
  const auto reject = [this](PacketError error) -> std::optional<fh::PacketIn> {
    ++stats_.packet_errors[static_cast<std::size_t>(error)];
    return std::nullopt;
  };

  ParsedPacket parsed = parse_packet(bytes);
  if (parsed.error != PacketError::None) return reject(parsed.error);
  if (session_ && *session_ != parsed.header.session) return reject(PacketError::SessionMismatch);
  if (!session_) session_ = parsed.header.session;  // the first sound packet names the session

  fh::PacketIn packet;
  packet.t = t;
  packet.line = line;

  if (parsed.header.count == kMoldHeartbeatCount) {
    packet.kind = fh::PacketKind::Heartbeat;
    packet.next_seq = parsed.header.seq;
    return packet;
  }
  if (parsed.header.count == kMoldEndOfSessionCount) {
    packet.kind = fh::PacketKind::EndOfSession;
    packet.next_seq = parsed.header.seq;
    return packet;
  }

  packet.kind = fh::PacketKind::Messages;
  packet.messages.reserve(parsed.blocks.size());
  for (std::size_t i = 0; i < parsed.blocks.size(); ++i) {
    const fh::Seq seq = parsed.header.seq + i;
    const ItchResult result = decode_itch(parsed.blocks[i]);
    switch (result.kind) {
      case ItchKind::Event: {
        fh::SequencedMessage message;
        message.seq = seq;
        message.event = result.event;
        message.event->seq = seq;
        message.digest = fh::fnv1a64(parsed.blocks[i]);
        packet.messages.push_back(message);
        ++stats_.messages;
        break;
      }
      case ItchKind::NoEffect:
      case ItchKind::Unknown: {
        // Unknown types still use up their sequence number, so they must be passed on.
        if (result.kind == ItchKind::Unknown) ++stats_.unknown_types;
        fh::SequencedMessage message;
        message.seq = seq;
        message.digest = fh::fnv1a64(parsed.blocks[i]);
        packet.messages.push_back(message);
        ++stats_.messages;
        break;
      }
      case ItchKind::Malformed:
        ++stats_.malformed_messages;
        ++stats_.itch_errors[static_cast<std::size_t>(result.error)];
        break;  // dropped: the core will see a hole at this sequence number
    }
  }
  return packet;
}

}  // namespace fh::nasdaq
