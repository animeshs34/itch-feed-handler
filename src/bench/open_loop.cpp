#include "fh/bench/open_loop.hpp"

#include <chrono>
#include <thread>

namespace fh::bench {

double OpenLoopResult::achieved_rate() const noexcept {
  return wall <= 0 ? 0.0
                   : static_cast<double>(measured) * static_cast<double>(kSecond) /
                         static_cast<double>(wall);
}

OpenLoopResult run_open_loop(const OpenLoopConfig& config, const ClockFn& now,
                             const WaitFn& wait_until, const WorkFn& work) {
  OpenLoopResult result;
  result.offered_rate =
      config.period > 0 ? static_cast<double>(kSecond) / static_cast<double>(config.period) : 0.0;

  const Nanos start = now();
  Nanos first_due = start;
  Nanos last_done = start;
  const std::uint64_t total = config.warmup + config.count;
  for (std::uint64_t i = 0; i < total; ++i) {
    const Nanos due = start + static_cast<Nanos>(i) * config.period;
    if (now() < due) wait_until(due);
    const Nanos started = now();
    work(i);
    const Nanos done = now();

    if (i < config.warmup) continue;
    if (result.measured == 0) first_due = due;
    ++result.measured;
    result.corrected.record(done - due);
    result.uncorrected.record(done - started);
    result.lag.record(started - due);
    if (started - due > config.late_threshold) ++result.late_starts;
    last_done = done;
  }
  result.wall = last_done - first_due;
  return result;
}

ClockFn monotonic_clock() {
  const auto origin = std::chrono::steady_clock::now();
  return [origin] {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() -
                                                                origin)
        .count();
  };
}

WaitFn spin_wait(const ClockFn& now, Nanos spin_window) {
  return [now, spin_window](Nanos until) {
    // Sleep while the wait is long (it frees the core), then spin for accuracy.
    const Nanos remaining = until - now();
    if (remaining > spin_window + 100 * kMicrosecond) {
      std::this_thread::sleep_for(std::chrono::nanoseconds(remaining - spin_window));
    }
    while (now() < until) {
    }
  };
}

}  // namespace fh::bench
