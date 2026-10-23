#ifndef FH_LINE_MONITOR_HPP_
#define FH_LINE_MONITOR_HPP_

#include <array>
#include <optional>

#include "fh/health.hpp"
#include "fh/messages.hpp"
#include "fh/time.hpp"

namespace fh {

struct LineMonitorConfig {
  // How long a line may say nothing (no messages and no heartbeats) before it
  // is considered down.
  Nanos silent_window{3 * kSecond};
};

// Watches whether each line is still being heard and works out the feed's health.
// Every function returns the new feed health if it changed, otherwise nothing.
class LineMonitor {
 public:
  explicit LineMonitor(const LineMonitorConfig& config) : config_(config) {}

  [[nodiscard]] std::optional<FeedHealth> heard(Line line, Nanos t);
  [[nodiscard]] std::optional<FeedHealth> tick(Nanos t);
  [[nodiscard]] std::optional<FeedHealth> end_of_session();

  [[nodiscard]] FeedHealth health() const noexcept { return health_; }
  [[nodiscard]] bool line_up(Line line) const noexcept { return up_[index_of(line)]; }

 private:
  void start_if_needed(Nanos t);
  [[nodiscard]] std::optional<FeedHealth> recompute();

  LineMonitorConfig config_;
  bool started_{false};
  bool ended_{false};
  std::array<Nanos, kLineCount> last_heard_{};
  std::array<bool, kLineCount> up_{true, true};
  FeedHealth health_{FeedHealth::Live};
};

}  // namespace fh

#endif  // FH_LINE_MONITOR_HPP_
