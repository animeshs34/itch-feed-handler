#include <catch2/catch_test_macros.hpp>

#include "fh/bench/recovery.hpp"

using namespace fh::bench;

TEST_CASE("a clean day has no recovery episodes", "[bench][recovery]") {
  const RecoveryResult r = bench_recovery(fh::sim::Tier::Clean, 1, 20);
  REQUIRE(r.scenarios == 20);
  REQUIRE(r.episodes == 0);
  REQUIRE(r.recovery.count() == 0);
  REQUIRE(r.unrecovered == 0);
}

TEST_CASE("a faulty day has episodes, and every one of them ends", "[bench][recovery]") {
  const RecoveryResult r = bench_recovery(fh::sim::Tier::Moderate, 1, 40);
  REQUIRE(r.episodes > 40);
  REQUIRE(r.recovery.count() == r.episodes);
  REQUIRE(r.unrecovered == 0);  // property P4: after the faults stop, every instrument is live
  REQUIRE(r.recovery.min() > 0);
}

TEST_CASE("recovery takes a fraction of the snapshot loop to a few loops", "[bench][recovery]") {
  const RecoveryResult r = bench_recovery(fh::sim::Tier::Moderate, 1, 40);
  REQUIRE(r.loop_period == 16 * fh::kMillisecond);  // 4 instruments at 4 ms each
  // A repair waits for the next snapshot of that instrument: at most one loop when
  // snapshots arrive, and a few when some are lost or late.
  REQUIRE(r.recovery.value_at_percentile(50) < 3 * r.loop_period);
  REQUIRE(r.recovery.max() < 20 * r.loop_period);
}

TEST_CASE("harsher faults make recovery slower", "[bench][recovery]") {
  const RecoveryResult mild = bench_recovery(fh::sim::Tier::Mild, 1, 60);
  const RecoveryResult brutal = bench_recovery(fh::sim::Tier::Brutal, 1, 60);
  REQUIRE(brutal.recovery.value_at_percentile(90) >= mild.recovery.value_at_percentile(90));
}

TEST_CASE("the recovery numbers are deterministic", "[bench][recovery]") {
  const RecoveryResult a = bench_recovery(fh::sim::Tier::Heavy, 5, 15);
  const RecoveryResult b = bench_recovery(fh::sim::Tier::Heavy, 5, 15);
  REQUIRE(a.episodes == b.episodes);
  REQUIRE(a.recovery.max() == b.recovery.max());
  REQUIRE(a.recovery.value_at_percentile(50) == b.recovery.value_at_percentile(50));
  REQUIRE(a.recovery.mean() == b.recovery.mean());
}
