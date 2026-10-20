#ifndef FH_TYPES_HPP_
#define FH_TYPES_HPP_

#include <cstdint>

namespace fh {

// Global feed sequence number. The first message of a session is 1.
using Seq = std::uint64_t;

// Instrument identifier. For Nasdaq ITCH this is the 16-bit stock locate code.
using InstrumentId = std::uint16_t;

// Exchange-assigned order reference, unique within a trading day.
using OrderId = std::uint64_t;

// Price as an integer mantissa. The decimal exponent is one feed-wide constant
// (-4 for ITCH), so 12345 with exponent -4 means 1.2345. Signed on purpose.
using Price = std::int64_t;

// A count of whole shares. Unsigned: a negative quantity has no meaning.
using Shares = std::uint64_t;

// The feed-wide decimal exponent of every Price.
using PriceExp = std::int8_t;

// The underlying values are the ASCII codes ITCH uses, so a decoded byte maps
// straight onto the enum and the canonical state encoding stays readable.
enum class Side : std::uint8_t { Buy = 'B', Sell = 'S' };

// True if `raw` is one of the two byte values a Side may take.
constexpr bool is_valid_side(std::uint8_t raw) noexcept { return raw == 'B' || raw == 'S'; }

}  // namespace fh

#endif  // FH_TYPES_HPP_
