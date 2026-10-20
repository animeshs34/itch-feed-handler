#ifndef FH_TESTS_SUPPORT_ITCH_GOLDEN_HPP_
#define FH_TESTS_SUPPORT_ITCH_GOLDEN_HPP_

#include "hex.hpp"

namespace fhtest {

// The golden vectors of the project spec, Appendix B.8: stock locate 1, tracking
// number 0, timestamp 09:30:00.000000000 plus an offset. They were generated from
// the layout tables, not captured from a real feed.

// Add order 101, buy 100 shares of "AAPL", price 150.2500 (field 1,502,500).
inline std::vector<std::uint8_t> golden_add() {
  return from_hex(
      "41 00 01 00 00 1f 1a ce d9 f0 00 00 00 00 00 00 00 00 65 42 00 00 00 64 "
      "41 41 50 4c 20 20 20 20 00 16 ed 24");
}
// Execute order 101 by 40 shares, match number 7001.
inline std::vector<std::uint8_t> golden_execute() {
  return from_hex(
      "45 00 01 00 00 1f 1a ce d9 f3 e8 00 00 00 00 00 00 00 65 00 00 00 28 "
      "00 00 00 00 00 00 1b 59");
}
// Cancel 10 shares of order 101.
inline std::vector<std::uint8_t> golden_cancel() {
  return from_hex("58 00 01 00 00 1f 1a ce d9 f7 d0 00 00 00 00 00 00 00 65 00 00 00 0a");
}
// Replace order 101 with 102, 70 shares at 150.3000 (field 1,503,000).
inline std::vector<std::uint8_t> golden_replace() {
  return from_hex(
      "55 00 01 00 00 1f 1a ce d9 fb b8 00 00 00 00 00 00 00 65 00 00 00 00 00 00 "
      "00 66 00 00 00 46 00 16 ef 18");
}
// Delete order 102.
inline std::vector<std::uint8_t> golden_delete() {
  return from_hex("44 00 01 00 00 1f 1a ce d9 ff a0 00 00 00 00 00 00 00 66");
}

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_ITCH_GOLDEN_HPP_
