#ifndef FH_STATE_CODEC_HPP_
#define FH_STATE_CODEC_HPP_

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

#include "fh/order_book.hpp"
#include "fh/types.hpp"

namespace fh {

// The whole feed's book state as of one sequence number: the unit that
// snapshots, checksums and later catch-up are all built on.
struct FeedState {
  PriceExp price_exp{-4};
  Seq seq{0};
  std::map<InstrumentId, InstrumentBook> books;

  friend bool operator==(const FeedState&, const FeedState&) = default;
};

// The canonical byte encoding. Equal states always give
// equal bytes: instruments and orders are written in ascending id order, all
// integers are little-endian and fixed width, and nothing is padded.
//
//   magic u32 'FHBK' | version u16 | price_exp i8 | seq u64 | instrument_count u32
//     per instrument: instrument_id u16 | order_count u32
//       per order: order_id u64 | side u8 | price i64 | shares u64
[[nodiscard]] std::vector<std::uint8_t> encode(const FeedState& state);

// Parses canonical bytes. Returns nullopt for anything that is not exactly
// what encode() could have produced: wrong magic or version, truncation,
// trailing bytes, ids out of order, a bad side byte, zero shares, or a level
// total that overflows. Never reads outside `bytes`.
[[nodiscard]] std::optional<FeedState> decode(std::span<const std::uint8_t> bytes);

}  // namespace fh

#endif  // FH_STATE_CODEC_HPP_
