#ifndef FH_OUTPUTS_HPP_
#define FH_OUTPUTS_HPP_

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <variant>

#include "fh/events.hpp"
#include "fh/health.hpp"
#include "fh/order_book.hpp"
#include "fh/types.hpp"

namespace fh {

enum class UpdateKind : std::uint8_t {
  Delta,  // one event applied to a live book
  Reset,  // the whole book replaced from a snapshot; starts a new epoch
};

// What the core publishes about one instrument's book.
//
//   seq    the global sequence number this update reflects
//   prev   the previous seq published for this instrument in this epoch
//          (nullopt on a Reset, which starts a fresh chain)
//   epoch  counts the Resets of this instrument
//
// A Delta carries the event and the new total of every price level it changed.
// A Reset carries every live order.
struct Update {
  UpdateKind kind{UpdateKind::Delta};
  InstrumentId inst{0};
  Seq seq{0};
  std::optional<Seq> prev;
  std::uint32_t epoch{0};
  BookEvent event;                       // Delta only
  std::array<TouchedLevel, 2> levels{};  // Delta only
  std::uint8_t level_count{0};           // Delta only
  std::map<OrderId, Order> orders;       // Reset only

  friend bool operator==(const Update&, const Update&) = default;
};

struct InstrumentHealthChange {
  InstrumentId inst{0};
  InstrumentHealth state{InstrumentHealth::Live};

  friend bool operator==(const InstrumentHealthChange&, const InstrumentHealthChange&) = default;
};

struct FeedHealthChange {
  FeedHealth state{FeedHealth::Live};

  friend bool operator==(const FeedHealthChange&, const FeedHealthChange&) = default;
};

enum class GapReason : std::uint8_t {
  Timeout,   // later messages arrived but the missing ones never did
  Tail,      // a heartbeat showed messages were sent that never arrived
  Overflow,  // the reorder buffer filled up before the wait was over
};

// Sequence numbers from..to (inclusive) will not be waited for any more.
struct GapDeclared {
  Seq from{0};
  Seq to{0};
  GapReason reason{GapReason::Timeout};

  friend bool operator==(const GapDeclared&, const GapDeclared&) = default;
};

enum class AlarmKind : std::uint8_t {
  LineMismatch,      // the same sequence number carried different bytes on A and B
  BookInconsistent,  // a message the book cannot apply: something was lost unnoticed
  ParkedOverflow,    // too many messages parked for an instrument that is recovering
  SnapshotRejected,  // a snapshot whose orders do not form a valid book
};

struct Alarm {
  AlarmKind kind{AlarmKind::LineMismatch};
  InstrumentId inst{0};  // 0 when not about one instrument
  Seq seq{0};            // the sequence number involved, if any

  friend bool operator==(const Alarm&, const Alarm&) = default;
};

using Output = std::variant<Update, InstrumentHealthChange, FeedHealthChange, GapDeclared, Alarm>;

}  // namespace fh

#endif  // FH_OUTPUTS_HPP_
