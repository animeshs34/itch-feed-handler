#ifndef FH_MESSAGES_HPP_
#define FH_MESSAGES_HPP_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "fh/events.hpp"
#include "fh/order_book.hpp"
#include "fh/time.hpp"
#include "fh/types.hpp"

namespace fh {

// Which of the two redundant lines a packet arrived on.
enum class Line : std::uint8_t { A = 0, B = 1 };
inline constexpr std::size_t kLineCount = 2;

constexpr std::size_t index_of(Line line) noexcept { return static_cast<std::size_t>(line); }

// One message of the main feed, after decoding.
struct SequencedMessage {
  Seq seq{0};
  // nullopt: the message uses up a sequence number but does not change any book
  // (a trade, a system event ...).
  std::optional<BookEvent> event;
  // Fingerprint of the original bytes. Used only to tell whether the copies that
  // arrived on line A and line B are identical.
  std::uint64_t digest{0};

  friend bool operator==(const SequencedMessage&, const SequencedMessage&) = default;
};

enum class PacketKind : std::uint8_t {
  Messages,      // carries messages
  Heartbeat,     // carries no messages; says what the publisher will send next
  EndOfSession,  // the publisher has finished; also says what it would send next
};

// One decoded packet of the main feed.
struct PacketIn {
  Nanos t{0};  // receive time
  Line line{Line::A};
  PacketKind kind{PacketKind::Messages};
  Seq next_seq{0};                         // Heartbeat and EndOfSession only
  std::vector<SequencedMessage> messages;  // Messages only, consecutive sequence numbers
};

// The items of a snapshot run, in the order they are sent.
struct SnapshotOpen {
  InstrumentId inst{0};
};
struct SnapshotOrder {
  InstrumentId inst{0};
  OrderId id{0};
  Order order;
};
struct SnapshotClose {
  Seq asof{0};  // the last main-feed sequence number the snapshot reflects
};
using SnapshotItem = std::variant<SnapshotOpen, SnapshotOrder, SnapshotClose>;

// One decoded packet of the snapshot stream. The snapshot stream has its own
// numbering, so a missing packet can be noticed.
struct SnapshotPacketIn {
  Nanos t{0};
  Seq stream_seq{0};  // snapshot-stream sequence number of the first item
  std::vector<SnapshotItem> items;
};

// Time passing with nothing received, so silence can be noticed.
struct TickIn {
  Nanos t{0};
};

using Input = std::variant<PacketIn, SnapshotPacketIn, TickIn>;

}  // namespace fh

#endif  // FH_MESSAGES_HPP_
