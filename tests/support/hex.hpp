#ifndef FH_TESTS_SUPPORT_HEX_HPP_
#define FH_TESTS_SUPPORT_HEX_HPP_

#include <cstdint>
#include <string_view>
#include <vector>

namespace fhtest {

// Bytes from text like "41 00 01 ff": pairs of hex digits, spaces ignored.
inline std::vector<std::uint8_t> from_hex(std::string_view text) {
  std::vector<std::uint8_t> out;
  int high = -1;
  for (const char c : text) {
    int digit = -1;
    if (c >= '0' && c <= '9') digit = c - '0';
    if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
    if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
    if (digit < 0) continue;  // spaces and anything else
    if (high < 0) {
      high = digit;
    } else {
      out.push_back(static_cast<std::uint8_t>(high * 16 + digit));
      high = -1;
    }
  }
  return out;
}

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_HEX_HPP_
