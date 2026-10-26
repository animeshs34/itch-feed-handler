#include "fh/nasdaq/snapshot_decoder.hpp"

#include <limits>

#include "fh/nasdaq/be_reader.hpp"
#include "fh/nasdaq/itch_decoder.hpp"

namespace fh::nasdaq {

std::optional<std::uint64_t> parse_resume_seq(std::span<const std::uint8_t> field) {
  std::size_t i = 0;
  while (i < field.size() && field[i] == ' ') ++i;  // leading spaces
  if (i == field.size()) return std::nullopt;       // no digits at all
  std::uint64_t value = 0;
  for (; i < field.size(); ++i) {
    if (field[i] < '0' || field[i] > '9') return std::nullopt;
    const std::uint64_t digit = static_cast<std::uint64_t>(field[i] - '0');
    if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) return std::nullopt;
    value = value * 10 + digit;
  }
  return value;
}

std::optional<fh::SnapshotPacketIn> SnapshotStreamDecoder::decode(
    fh::Nanos t, std::span<const std::uint8_t> bytes) {
  ++stats_.packets;
  const auto reject = [this](PacketError error) -> std::optional<fh::SnapshotPacketIn> {
    ++stats_.packet_errors[static_cast<std::size_t>(error)];
    return std::nullopt;
  };

  ParsedPacket parsed = parse_packet(bytes);
  if (parsed.error != PacketError::None) return reject(parsed.error);
  if (session_ && *session_ != parsed.header.session) return reject(PacketError::SessionMismatch);

  fh::SnapshotPacketIn packet;
  packet.t = t;
  packet.stream_seq = parsed.header.seq;

  for (const auto block : parsed.blocks) {
    if (block.empty()) return reject(PacketError::UnexpectedMessage);
    const char type = static_cast<char>(block[0]);

    if (type == 'G') {
      if (block.size() != 21) return reject(PacketError::UnexpectedMessage);
      const auto resume = parse_resume_seq(block.subspan(1));
      if (!resume || *resume == 0) return reject(PacketError::UnexpectedMessage);
      packet.items.emplace_back(fh::SnapshotClose{*resume - 1});  // as-of = resume - 1
    } else if (type == 'R') {
      const ItchResult r = decode_itch(block);
      if (r.kind != ItchKind::NoEffect) return reject(PacketError::UnexpectedMessage);
      BeReader in(block);
      (void)in.skip(1);
      packet.items.emplace_back(fh::SnapshotOpen{*in.u16()});  // the stock locate
    } else if (type == 'A' || type == 'F') {
      const ItchResult r = decode_itch(block);
      if (r.kind != ItchKind::Event) return reject(PacketError::UnexpectedMessage);
      const fh::BookEvent& e = r.event;
      packet.items.emplace_back(
          fh::SnapshotOrder{e.inst, e.order, fh::Order{e.side, e.price, e.shares}});
    } else {
      return reject(PacketError::UnexpectedMessage);
    }
  }

  if (!session_) session_ = parsed.header.session;
  stats_.messages += packet.items.size();
  return packet;
}

}  // namespace fh::nasdaq
