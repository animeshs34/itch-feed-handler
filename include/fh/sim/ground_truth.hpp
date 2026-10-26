#ifndef FH_SIM_GROUND_TRUTH_HPP_
#define FH_SIM_GROUND_TRUTH_HPP_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>

#include "fh/events.hpp"
#include "fh/types.hpp"

namespace fh::sim {

// One message of the true main feed, before any fault. Message number n is
// `messages[n - 1]` and carries sequence number n.
//
// A message either changes a book (`event` is set) or does not. A message that
// does not is a "filler": a system event, a stock directory, a trade ... The
// `filler` letter says which ITCH type to write, and `code` is the event code
// when that type is a system event ('S').
//
// `raw` is empty for a synthetic message, which is written from the fields above.
// A message read from a recorded file keeps its exact bytes here, and those bytes go
// on the wire unchanged, so the handler sees what Nasdaq really sent.
struct GroundMessage {
  Seq seq{0};
  std::optional<BookEvent> event;
  char filler{'N'};
  char code{' '};
  InstrumentId locate{0};  // the instrument a filler is about; 0 for market-wide ones
  std::vector<std::uint8_t> raw;

  friend bool operator==(const GroundMessage&, const GroundMessage&) = default;
};

// Everything the true feed said, in order. This is the answer key for a run: the
// oracle replays `events()` to know what every book must hold.
struct GroundTruth {
  std::vector<GroundMessage> messages;

  [[nodiscard]] Seq last_seq() const noexcept { return messages.size(); }

  // The book events only, in sequence order, with their feed sequence numbers.
  [[nodiscard]] std::vector<BookEvent> events() const;

  // Every instrument that has at least one book event. An instrument that only appears in
  // fillers has no book and is never part of the expected state.
  [[nodiscard]] std::set<InstrumentId> instruments() const;

  friend bool operator==(const GroundTruth&, const GroundTruth&) = default;
};

}  // namespace fh::sim

#endif  // FH_SIM_GROUND_TRUTH_HPP_
