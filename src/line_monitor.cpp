#include "fh/line_monitor.hpp"

namespace fh {

void LineMonitor::start_if_needed(Nanos t) {
  if (started_) return;
  started_ = true;
  last_heard_.fill(t);  // the clock starts when the first input arrives
}

std::optional<FeedHealth> LineMonitor::heard(Line line, Nanos t) {
  start_if_needed(t);
  last_heard_[index_of(line)] = t;
  up_[index_of(line)] = true;
  return recompute();
}

std::optional<FeedHealth> LineMonitor::tick(Nanos t) {
  start_if_needed(t);
  for (std::size_t i = 0; i < kLineCount; ++i) {
    if (up_[i] && t - last_heard_[i] >= config_.silent_window) up_[i] = false;
  }
  return recompute();
}

std::optional<FeedHealth> LineMonitor::end_of_session() {
  ended_ = true;
  return recompute();
}

std::optional<FeedHealth> LineMonitor::recompute() {
  FeedHealth now = FeedHealth::Live;
  if (ended_) {
    now = FeedHealth::Ended;
  } else {
    const int down = (up_[0] ? 0 : 1) + (up_[1] ? 0 : 1);
    now = down == 0 ? FeedHealth::Live : down == 1 ? FeedHealth::Degraded : FeedHealth::Stale;
  }
  if (now == health_) return std::nullopt;
  health_ = now;
  return now;
}

}  // namespace fh
