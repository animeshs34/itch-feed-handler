#ifndef FH_TIME_HPP_
#define FH_TIME_HPP_

#include <cstdint>

namespace fh {

// A point in time or a duration, in nanoseconds. The core never reads a clock:
// every input carries its own timestamp of this type.
// Signed on purpose, so subtracting two times can never wrap around.
using Nanos = std::int64_t;

inline constexpr Nanos kMicrosecond = 1'000;
inline constexpr Nanos kMillisecond = 1'000'000;
inline constexpr Nanos kSecond = 1'000'000'000;

}  // namespace fh

#endif  // FH_TIME_HPP_
