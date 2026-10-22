#ifndef FH_STATE_FORMAT_HPP_
#define FH_STATE_FORMAT_HPP_

#include <cstdint>

// Private to the library: the constants the encoder and the decoder must agree
// on. Kept in one place so they cannot drift apart.
namespace fh::detail {

inline constexpr std::uint32_t kStateMagic = 0x4B424846;  // bytes 46 48 42 4B = "FHBK"
inline constexpr std::uint16_t kStateVersion = 1;

}  // namespace fh::detail

#endif  // FH_STATE_FORMAT_HPP_
