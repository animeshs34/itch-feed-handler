#ifndef FH_CHECKSUM_HPP_
#define FH_CHECKSUM_HPP_

#include <cstdint>
#include <span>

#include "fh/state_codec.hpp"

namespace fh {

// FNV-1a, 64 bit. A non-cryptographic fingerprint: it detects accidental
// difference and corruption, and gives no protection against an attacker.
// The algorithm is part of the state format; changing it means a new version.
constexpr std::uint64_t fnv1a64(std::span<const std::uint8_t> bytes) noexcept {
  std::uint64_t hash = 14695981039346656037ULL;  // offset basis
  for (const std::uint8_t byte : bytes) {
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= 1099511628211ULL;  // FNV prime
  }
  return hash;
}

// Fingerprint of a feed state: the hash of its canonical encoding.
inline std::uint64_t checksum(const FeedState& state) { return fnv1a64(encode(state)); }

}  // namespace fh

#endif  // FH_CHECKSUM_HPP_
