#ifndef FH_HEALTH_HPP_
#define FH_HEALTH_HPP_

#include <cstdint>

namespace fh {

// Whether one instrument's book can be trusted right now.
//   Live     the book is correct and up to date; updates are published
//   Syncing  the book is not trusted; nothing is published until a snapshot repairs it
enum class InstrumentHealth : std::uint8_t { Live, Syncing };

// Whether the feed itself is arriving.
//   Live      both lines are being heard
//   Degraded  one line has gone silent (the other still carries everything)
//   Stale     both lines have gone silent
//   Ended     the publisher announced the end of the session; silence is expected
enum class FeedHealth : std::uint8_t { Live, Degraded, Stale, Ended };

}  // namespace fh

#endif  // FH_HEALTH_HPP_
