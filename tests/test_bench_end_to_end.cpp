#include <catch2/catch_test_macros.hpp>

#include "fh/bench/end_to_end.hpp"

using namespace fh::bench;

TEST_CASE("the end-to-end benchmark delivers every datagram through real sockets",
          "[bench][workload]") {
  const Workload w = make_workload(3000, 5, 1, 1);
  std::string error;
  const auto r = bench_end_to_end(w, 20000.0, 400, 100, error);
  INFO(error);
  REQUIRE(r.has_value());
  REQUIRE(r->sent == 400);
  REQUIRE(r->received == 400);
  REQUIRE(r->latency.count() == 400);
  REQUIRE(r->latency.min() > 0);  // a datagram cannot arrive before it was sent
}

TEST_CASE("the end-to-end benchmark refuses a packed workload and too many items",
          "[bench][workload]") {
  std::string error;
  const Workload packed = make_workload(3000, 5, 1, 4);
  REQUIRE_FALSE(bench_end_to_end(packed, 1000.0, 100, 10, error).has_value());
  REQUIRE_FALSE(error.empty());
  const Workload w = make_workload(3000, 5, 1, 1);
  REQUIRE_FALSE(bench_end_to_end(w, 1000.0, w.datagrams.size() + 5, 0, error).has_value());
}
