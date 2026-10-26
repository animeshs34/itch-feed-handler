#ifndef FH_NASDAQ_ITCH_LAYOUT_HPP_
#define FH_NASDAQ_ITCH_LAYOUT_HPP_

#include <array>
#include <cstddef>
#include <cstdint>

namespace fh::nasdaq {

// What we know about each ITCH 5.0 message type before looking inside it: how
// long it must be (in bytes, including the type byte), and whether it changes
// an order book. Taken from the specification's message tables (Appendix B.3
// of the project spec).
struct MessageSpec {
  char type;
  std::uint8_t length;
  bool affects_book;
};

inline constexpr std::array<MessageSpec, 23> kMessageSpecs{{
    // System and reference messages
    {'S', 12, false},  // System Event
    {'R', 39, false},  // Stock Directory
    {'H', 25, false},  // Stock Trading Action
    {'Y', 20, false},  // Reg SHO Short Sale Price Test Restricted Indicator
    {'L', 26, false},  // Market Participant Position
    {'V', 35, false},  // MWCB Decline Level
    {'W', 12, false},  // MWCB Status
    {'K', 28, false},  // IPO Quoting Period Update
    {'J', 35, false},  // LULD Auction Collar
    {'h', 21, false},  // Operational Halt
    // Order book messages
    {'A', 36, true},  // Add Order, no MPID
    {'F', 40, true},  // Add Order with MPID
    {'E', 31, true},  // Order Executed
    {'C', 36, true},  // Order Executed With Price
    {'X', 23, true},  // Order Cancel
    {'D', 19, true},  // Order Delete
    {'U', 35, true},  // Order Replace
    // Trade and auction messages
    {'P', 44, false},  // Trade (non-cross)
    {'Q', 40, false},  // Cross Trade
    {'B', 19, false},  // Broken Trade
    {'I', 50, false},  // Net Order Imbalance Indicator
    {'N', 20, false},  // Retail Price Improvement Indicator
    {'O', 48, false},  // Direct Listing with Capital Raise Price Discovery
}};

// The spec entry for a message type, or nullptr if the type is not one we know.
constexpr const MessageSpec* find_spec(char type) noexcept {
  for (const MessageSpec& spec : kMessageSpecs) {
    if (spec.type == type) return &spec;
  }
  return nullptr;
}

// The largest value a Price(4) field may hold: 200,000.0000 (hex 77359400).
inline constexpr std::uint32_t kMaxPrice4 = 2'000'000'000;

// Fixed offsets shared by every message: type (0), stock locate (1, 2 bytes),
// tracking number (3, 2 bytes), timestamp (5, 6 bytes). Message-specific fields
// start at 11.
inline constexpr std::size_t kHeaderBytes = 11;

}  // namespace fh::nasdaq

#endif  // FH_NASDAQ_ITCH_LAYOUT_HPP_
