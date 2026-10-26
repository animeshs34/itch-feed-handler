#ifndef FH_NASDAQ_MOLDUDP64_HPP_
#define FH_NASDAQ_MOLDUDP64_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "fh/messages.hpp"
#include "fh/nasdaq/itch_decoder.hpp"
#include "fh/time.hpp"
#include "fh/types.hpp"

namespace fh::nasdaq {

// MoldUDP64 version 1.00: a 20 byte header (session 10 bytes, sequence number of
// the first message 8 bytes, message count 2 bytes) followed by `count` blocks,
// each a 2 byte length and that many bytes of message. A count of 0 is a
// heartbeat; 0xFFFF is the end of the session. All numbers are big-endian.
inline constexpr std::size_t kMoldHeaderBytes = 20;
inline constexpr std::uint16_t kMoldHeartbeatCount = 0;
inline constexpr std::uint16_t kMoldEndOfSessionCount = 0xFFFF;

using SessionId = std::array<char, 10>;

// A session id from text, padded on the right with spaces (or cut to 10).
[[nodiscard]] SessionId make_session(std::string_view name);

struct MoldHeader {
  SessionId session{};
  fh::Seq seq{0};  // sequence number of the first message (or the next one, for heartbeats)
  std::uint16_t count{0};
};

// Why a whole packet was thrown away.
enum class PacketError : std::uint8_t {
  None,
  TooShort,           // fewer than 20 bytes
  BlockOverrun,       // a block's length runs past the end of the packet
  TrailingBytes,      // bytes left over after the last block
  SessionMismatch,    // from a different session than the one in progress
  HeartbeatHasBody,   // a heartbeat or end-of-session packet with extra bytes
  BadSequence,        // the sequence numbers would overflow
  UnexpectedMessage,  // a message the snapshot stream never carries
};
inline constexpr std::size_t kPacketErrorCount = 8;

// A packet's structure, before looking inside any message.
struct ParsedPacket {
  MoldHeader header;
  std::vector<std::span<const std::uint8_t>> blocks;  // views into the original bytes
  PacketError error{PacketError::None};
};

// Splits a packet into header and message blocks. Checks only the structure:
// enough bytes, lengths that fit, nothing left over, heartbeats with no body.
[[nodiscard]] ParsedPacket parse_packet(std::span<const std::uint8_t> bytes);

// Counts of what the decoders saw and refused. Nothing here changes behaviour; it
// is for reports and tests.
struct DecoderStats {
  std::uint64_t packets{0};             // packets offered
  std::uint64_t messages{0};            // messages passed on
  std::uint64_t unknown_types{0};       // well-formed messages of a type we do not know
  std::uint64_t malformed_messages{0};  // single messages dropped (the rest of the packet kept)
  std::array<std::uint64_t, kItchErrorCount> itch_errors{};
  std::array<std::uint64_t, kPacketErrorCount> packet_errors{};
};

// Turns main-feed datagrams into decoded packets for the core.
//
// A packet whose structure is broken, or that belongs to another session, is
// dropped whole. A packet with one bad message keeps the others: the bad one
// simply never reaches the core, which sees a hole in the numbering and deals
// with it like any other loss.
class PacketDecoder {
 public:
  [[nodiscard]] std::optional<fh::PacketIn> decode(fh::Nanos t, fh::Line line,
                                                   std::span<const std::uint8_t> bytes);

  [[nodiscard]] const DecoderStats& stats() const noexcept { return stats_; }
  [[nodiscard]] const std::optional<SessionId>& session() const noexcept { return session_; }

 private:
  std::optional<SessionId> session_;
  DecoderStats stats_;
};

}  // namespace fh::nasdaq

#endif  // FH_NASDAQ_MOLDUDP64_HPP_
