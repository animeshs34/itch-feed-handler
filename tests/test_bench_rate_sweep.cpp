#include <catch2/catch_test_macros.hpp>

#include "fh/bench/rate_sweep.hpp"

using namespace fh::bench;

TEST_CASE("the open-loop benchmark records the items it was asked for", "[bench][workload]") {
  const Workload w = make_workload(3000, 5, 1, 1);
  const OpenLoopResult r = bench_open_loop(w, Stage::Handler, 20000.0, 500, 100);
  REQUIRE(r.measured == 500);
  REQUIRE(r.corrected.count() == 500);
  REQUIRE(r.offered_rate == 20000.0);
  REQUIRE(r.corrected.min() >= 0);
  // corrected includes waiting, so it can never be below the service time
  REQUIRE(r.corrected.value_at_percentile(50) >= r.uncorrected.value_at_percentile(50));
}

TEST_CASE("the core-only open loop times the core on decoded datagrams", "[bench][workload]") {
  const Workload w = make_workload(3000, 5, 1, 1);
  const OpenLoopResult r = bench_open_loop(w, Stage::Core, 20000.0, 500, 100);
  REQUIRE(r.measured == 500);
  REQUIRE(r.corrected.count() == 500);
  REQUIRE(r.corrected.value_at_percentile(50) >= r.uncorrected.value_at_percentile(50));
}

TEST_CASE(
    "past the rate it can sustain the handler's latency grows while its service time does not",
    "[bench][workload]") {
  const Workload w = make_workload(20000, 10, 1, 1);
  // Ask for far more than any machine can do: 20 million datagrams a second.
  const OpenLoopResult r = bench_open_loop(w, Stage::Handler, 20e6, 5000, 500);
  REQUIRE(r.achieved_rate() < 0.9 * r.offered_rate);
  REQUIRE(r.corrected.value_at_percentile(99) > 20 * r.uncorrected.value_at_percentile(99));
}
