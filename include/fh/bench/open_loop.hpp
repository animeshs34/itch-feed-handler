#ifndef FH_BENCH_OPEN_LOOP_HPP_
#define FH_BENCH_OPEN_LOOP_HPP_

#include <cstdint>
#include <functional>

#include "fh/bench/histogram.hpp"
#include "fh/time.hpp"

namespace fh::bench {

// An open-loop load generator: work item i is *due* at start + i * period, whether or not
// the previous item has finished. That is how a real feed behaves (the exchange does not
// wait for you), and it is the only honest way to measure latency under load.
//
// A closed loop (start the next item when the last one finishes) hides queueing: if one
// item stalls for 100 ms, a closed loop records one slow sample, while a real feed would
// have delivered a hundred messages in that time and every one of them would have waited.
// Gil Tene calls the closed-loop mistake "coordinated omission". The latency of an item
// here is measured from when it was DUE, not from when it started:
//
//   corrected   = done - due        includes the time the item waited behind earlier ones
//   uncorrected = done - started    the service time alone; what a closed loop would report
//
// Both are recorded so that a report can show how much the correction matters.
struct OpenLoopConfig {
  std::uint64_t count{100000};
  Nanos period{10 * kMicrosecond};  // 100,000 items a second
  std::uint64_t warmup{0};          // items run first and not recorded
  Nanos late_threshold{
      kMicrosecond};  // an item that starts later than this after its due time is "late"
};

struct OpenLoopResult {
  LogLinearHistogram corrected;
  LogLinearHistogram uncorrected;
  LogLinearHistogram lag;        // started - due, per item: how far behind the schedule
  Nanos wall{0};                 // first due time to last completion
  std::uint64_t late_starts{0};  // items that could not start on time
  std::uint64_t measured{0};

  // Items finished per second over the whole run.
  [[nodiscard]] double achieved_rate() const noexcept;
  // Items per second that were offered (1 / period).
  double offered_rate{0};
};

using ClockFn = std::function<Nanos()>;
using WaitFn = std::function<void(Nanos until)>;
using WorkFn = std::function<void(std::uint64_t index)>;

// Runs the schedule. `now` reads the clock, `wait_until(t)` returns once the clock has
// reached `t` (a real run spins or sleeps; a test advances a fake clock), and `work(i)`
// does item i. All three are injected so that the logic can be tested without any real
// time passing.
[[nodiscard]] OpenLoopResult run_open_loop(const OpenLoopConfig& config, const ClockFn& now,
                                           const WaitFn& wait_until, const WorkFn& work);

// The real thing: the monotonic clock, and a wait that sleeps until shortly before the due
// time and then spins on the clock for the rest, so that items start within about a
// microsecond of their due time.
[[nodiscard]] ClockFn monotonic_clock();
[[nodiscard]] WaitFn spin_wait(const ClockFn& now, Nanos spin_window = 50 * kMicrosecond);

}  // namespace fh::bench

#endif  // FH_BENCH_OPEN_LOOP_HPP_
