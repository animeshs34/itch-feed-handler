#ifndef FH_TESTS_SUPPORT_PRNG_HPP_
#define FH_TESTS_SUPPORT_PRNG_HPP_

#include <cassert>
#include <cstdint>

namespace fhtest {

// SplitMix64: a tiny, well-known 64-bit generator (Steele, Lea, Flood 2014).
// Written out here, with its own range functions, because the standard
// library's distributions are allowed to give different numbers on different
// compilers, and our tests must produce the same data everywhere from a seed.
class SplitMix64 {
 public:
  explicit SplitMix64(std::uint64_t seed) noexcept : state_(seed) {}

  std::uint64_t next() noexcept {
    state_ += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state_;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }

  // Uniform in [0, n). Precondition: n > 0. Rejects the few draws that would
  // make small values slightly more likely than large ones (modulo bias).
  std::uint64_t below(std::uint64_t n) noexcept {
    assert(n > 0);
    const std::uint64_t threshold = (0 - n) % n;  // 2^64 mod n
    std::uint64_t r = next();
    while (r < threshold) r = next();
    return r % n;
  }

  // Uniform in [lo, hi], both included. Precondition: lo <= hi and the span is small.
  std::int64_t between(std::int64_t lo, std::int64_t hi) noexcept {
    assert(lo <= hi);
    const std::uint64_t span = static_cast<std::uint64_t>(hi) - static_cast<std::uint64_t>(lo) + 1;
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(lo) + below(span));
  }

 private:
  std::uint64_t state_;
};

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_PRNG_HPP_
