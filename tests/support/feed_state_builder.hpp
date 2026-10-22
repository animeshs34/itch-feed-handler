#ifndef FH_TESTS_SUPPORT_FEED_STATE_BUILDER_HPP_
#define FH_TESTS_SUPPORT_FEED_STATE_BUILDER_HPP_

#include <vector>

#include "fh/events.hpp"
#include "fh/state_codec.hpp"

namespace fhtest {

// Applies a list of events to a fresh FeedState, creating each instrument's
// book on first use. Returns false if any event is rejected.
inline bool build_state(const std::vector<fh::BookEvent>& events, fh::FeedState& out) {
  out = fh::FeedState{};
  for (const fh::BookEvent& e : events) {
    if (!out.books[e.inst].apply(e).ok()) return false;
    out.seq = e.seq;
  }
  return true;
}

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_FEED_STATE_BUILDER_HPP_
