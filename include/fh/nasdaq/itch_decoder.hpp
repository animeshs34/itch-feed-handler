#ifndef FH_NASDAQ_ITCH_DECODER_HPP_
#define FH_NASDAQ_ITCH_DECODER_HPP_

#include <cstdint>
#include <span>

#include "fh/events.hpp"

namespace fh::nasdaq {

// Why one message was rejected.
enum class ItchError : std::uint8_t {
  None,
  Empty,        // no bytes at all
  WrongLength,  // a known type, but not the length the specification gives
  BadSide,      // buy/sell indicator is neither 'B' nor 'S'
  BadPrice,     // a price above the 200,000.0000 limit
  ZeroShares,   // a share count of zero where the book needs at least one
  BadFlag,      // a flag field with a value the specification does not allow
};
inline constexpr std::size_t kItchErrorCount = 7;

enum class ItchKind : std::uint8_t {
  Event,      // changes a book: `event` is filled in (its `seq` is left at 0)
  NoEffect,   // a known type that does not change any book
  Unknown,    // a type we do not know; it still uses up a sequence number
  Malformed,  // do not trust it: see `error`
};

struct ItchResult {
  ItchKind kind{ItchKind::Malformed};
  ItchError error{ItchError::None};
  char type{0};
  fh::BookEvent event;  // meaningful only when kind == Event
};

// Decodes one ITCH 5.0 message (the bytes of exactly one message, without the
// MoldUDP64 length prefix).
//
// Book-changing messages become a BookEvent with the stock locate as the
// instrument id:
//   A, F  Add          E, C, X  Reduce (an execution and a cancel both remove shares)
//   D     Delete       U        Replace
// The sequence number is not known here; the packet decoder fills it in.
[[nodiscard]] ItchResult decode_itch(std::span<const std::uint8_t> message);

}  // namespace fh::nasdaq

#endif  // FH_NASDAQ_ITCH_DECODER_HPP_
