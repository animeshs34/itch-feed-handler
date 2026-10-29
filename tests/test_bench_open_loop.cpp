#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "fh/bench/open_loop.hpp"

namespace {

// A clock that only moves when told to. `work` advances it by the item's service time.
struct FakeClock {
  fh::Nanos t{0};
  fh::bench::ClockFn clock() {
    return [this] { return t; };
  }
  fh::bench::WaitFn wait() {
    return [this](fh::Nanos until) {
      if (until > t) t = until;
    };
  }
};

}  // namespace

TEST_CASE("an idle system measures only the service time", "[bench][open-loop]") {
  FakeClock fake;
  fh::bench::OpenLoopConfig config;
  config.count = 1000;
  config.period = fh::kMillisecond;
  const auto r = fh::bench::run_open_loop(config, fake.clock(), fake.wait(),
                                          [&](std::uint64_t) { fake.t += 100 * fh::kMicrosecond; });
  REQUIRE(r.measured == 1000);
  // Each item waits until it is due, runs 100 us, and is done: corrected == uncorrected == 100 us.
  REQUIRE(r.corrected.min() == 100 * fh::kMicrosecond);
  REQUIRE(r.corrected.max() == 100 * fh::kMicrosecond);
  REQUIRE(r.uncorrected.max() == 100 * fh::kMicrosecond);
  REQUIRE(r.late_starts == 0);
  REQUIRE(r.lag.max() == 0);
}

TEST_CASE("one long stall delays every item that was due during it", "[bench][open-loop]") {
  // 1000 items due a millisecond apart; item 100 takes 200 ms. A real feed would have sent
  // 200 more messages during the stall, and every one of them waits.
  FakeClock fake;
  fh::bench::OpenLoopConfig config;
  config.count = 1000;
  config.period = fh::kMillisecond;
  const auto r = fh::bench::run_open_loop(config, fake.clock(), fake.wait(), [&](std::uint64_t i) {
    fake.t += (i == 100 ? 200 * fh::kMillisecond : 100 * fh::kMicrosecond);
  });

  // Uncorrected (a closed loop's view): one slow sample, the rest fast.
  REQUIRE(r.uncorrected.max() == 200 * fh::kMillisecond);
  REQUIRE(r.uncorrected.value_at_percentile(99) < 2 * fh::kMillisecond);

  // Corrected: the items due during the stall all waited, so about 200 samples are slow.
  std::uint64_t slow = 0;
  for (double p = 0; p <= 100; p += 0.1) {
    if (r.corrected.value_at_percentile(p) > 10 * fh::kMillisecond) ++slow;
  }
  REQUIRE(slow > 100);  // about a fifth of the percentile grid (about 200 of 1000 items)
  REQUIRE(r.corrected.value_at_percentile(90) > 50 * fh::kMillisecond);
  REQUIRE(r.corrected.value_at_percentile(99) > 100 * fh::kMillisecond);
  REQUIRE(r.late_starts > 150);
  REQUIRE(r.lag.max() > 190 * fh::kMillisecond);
}

TEST_CASE("the backlog drains: later items catch up and stop being late", "[bench][open-loop]") {
  FakeClock fake;
  fh::bench::OpenLoopConfig config;
  config.count = 2000;
  config.period = fh::kMillisecond;
  std::vector<fh::Nanos> started;
  const auto r = fh::bench::run_open_loop(config, fake.clock(), fake.wait(), [&](std::uint64_t i) {
    started.push_back(fake.t);
    fake.t += (i == 10 ? 50 * fh::kMillisecond : 100 * fh::kMicrosecond);
  });
  // After the stall the loop runs items back to back (each takes 0.1 ms against a 1 ms period)
  // until it has caught up, then starts them on time again.
  REQUIRE(started[10] == 10 * fh::kMillisecond);
  REQUIRE(started[11] == 60 * fh::kMillisecond);  // had to wait for item 10
  REQUIRE(started[60] > 60 * fh::kMillisecond);
  REQUIRE(started[200] == 200 * fh::kMillisecond);  // on time again
  REQUIRE(r.corrected.max() >= 50 * fh::kMillisecond);
}

TEST_CASE("an overloaded system shows latency growing without bound", "[bench][open-loop]") {
  // Items arrive every 1 ms and each takes 2 ms: the queue grows by 1 ms per item.
  FakeClock fake;
  fh::bench::OpenLoopConfig config;
  config.count = 500;
  config.period = fh::kMillisecond;
  const auto r = fh::bench::run_open_loop(config, fake.clock(), fake.wait(),
                                          [&](std::uint64_t) { fake.t += 2 * fh::kMillisecond; });
  REQUIRE(r.uncorrected.max() == 2 * fh::kMillisecond);  // the service time never changes ...
  REQUIRE(r.corrected.max() >=
          499 * fh::kMillisecond);  // ... but the last item waited for 499 others
  REQUIRE(r.achieved_rate() < 0.6 * r.offered_rate);
  REQUIRE(r.offered_rate == 1000.0);
}

TEST_CASE("warm-up items run but are not recorded", "[bench][open-loop]") {
  FakeClock fake;
  fh::bench::OpenLoopConfig config;
  config.count = 100;
  config.warmup = 50;
  config.period = fh::kMillisecond;
  std::uint64_t calls = 0;
  const auto r = fh::bench::run_open_loop(config, fake.clock(), fake.wait(), [&](std::uint64_t) {
    ++calls;
    fake.t += 10 * fh::kMicrosecond;
  });
  REQUIRE(calls == 150);
  REQUIRE(r.measured == 100);
  REQUIRE(r.corrected.count() == 100);
}

TEST_CASE("achieved rate is items over wall time", "[bench][open-loop]") {
  FakeClock fake;
  fh::bench::OpenLoopConfig config;
  config.count = 1000;
  config.period = fh::kMillisecond;
  const auto r = fh::bench::run_open_loop(config, fake.clock(), fake.wait(),
                                          [&](std::uint64_t) { fake.t += 10 * fh::kMicrosecond; });
  // 1000 items, one per millisecond: about 1000 per second.
  REQUIRE(r.achieved_rate() > 990.0);
  REQUIRE(r.achieved_rate() < 1010.0);
}

TEST_CASE("the real clock and wait start items close to their due time", "[bench][open-loop]") {
  const auto now = fh::bench::monotonic_clock();
  const auto wait = fh::bench::spin_wait(now);
  fh::bench::OpenLoopConfig config;
  config.count = 200;
  config.period = 200 * fh::kMicrosecond;
  const auto r = fh::bench::run_open_loop(config, now, wait, [](std::uint64_t) {});
  REQUIRE(r.measured == 200);
  // Doing nothing takes almost no time; a loaded machine can delay a start, never advance it.
  REQUIRE(r.lag.min() >= 0);
  REQUIRE(r.lag.value_at_percentile(50) < 2 * fh::kMillisecond);
  REQUIRE(r.wall >= 199 * 200 * fh::kMicrosecond);
}

TEST_CASE("the real wait never returns early", "[bench][open-loop]") {
  const auto now = fh::bench::monotonic_clock();
  const auto wait = fh::bench::spin_wait(now);
  for (const fh::Nanos delay :
       {10 * fh::kMicrosecond, 300 * fh::kMicrosecond, 5 * fh::kMillisecond}) {
    const fh::Nanos target = now() + delay;
    wait(target);
    REQUIRE(now() >= target);
  }
}
