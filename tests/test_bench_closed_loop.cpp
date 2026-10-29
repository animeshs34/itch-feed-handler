#include <catch2/catch_test_macros.hpp>

#include "fh/bench/closed_loop.hpp"

using namespace fh::bench;

TEST_CASE("the closed-loop benchmark times every datagram of every pass", "[bench][workload]") {
  const Workload w = make_workload(3000, 5, 1, 1);
  const ClosedLoopResult r = bench_handler_closed_loop(w, 2);
  REQUIRE(r.service.count() == 2 * w.datagrams.size());
  REQUIRE(r.messages == w.messages);
  REQUIRE(r.messages_per_second > 1000.0);
  REQUIRE(r.outputs > 0);
  const ClosedLoopResult core = bench_core_closed_loop(w, 2);
  REQUIRE(core.service.count() == 2 * w.datagrams.size());
  REQUIRE(core.outputs ==
          r.outputs);  // decoding first changes nothing about what the core publishes
}
