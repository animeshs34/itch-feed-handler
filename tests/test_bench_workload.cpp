#include <catch2/catch_test_macros.hpp>

#include "fh/bench/workload.hpp"
#include "fh/nasdaq/handler.hpp"
#include "fh/sim/day.hpp"
#include "fh/sim/wire.hpp"

using namespace fh::bench;

TEST_CASE("a workload is deterministic for a seed", "[bench][workload]") {
  const Workload a = make_workload(2000, 5, 7, 1);
  const Workload b = make_workload(2000, 5, 7, 1);
  const Workload c = make_workload(2000, 5, 8, 1);
  REQUIRE(a.datagrams == b.datagrams);
  REQUIRE(a.datagrams != c.datagrams);
}

TEST_CASE("with one message per datagram the numbering is one to n", "[bench][workload]") {
  const Workload w = make_workload(3000, 6, 1, 1);
  REQUIRE(w.datagrams.size() == w.messages);
  REQUIRE(w.first_seq.size() == w.messages);
  for (std::size_t i = 0; i < w.first_seq.size(); ++i) REQUIRE(w.first_seq[i] == i + 1);
  REQUIRE(w.bytes > 0);
}

TEST_CASE("packing puts several messages in a datagram", "[bench][workload]") {
  const Workload w = make_workload(3000, 6, 1, 4);
  REQUIRE(w.datagrams.size() < w.messages);
  REQUIRE(w.datagrams.size() * 4 >= w.messages);
}

TEST_CASE("the handler digests a workload cleanly: no gaps and every book live",
          "[bench][workload]") {
  for (const unsigned pack : {1U, 4U}) {
    const Workload w = make_workload(5000, 8, 3, pack);
    fh::nasdaq::Handler handler{fh::CoreConfig{}};
    std::vector<fh::Output> out;
    fh::Nanos t = 0;
    for (const auto& d : w.datagrams) {
      t += 100 * fh::kMicrosecond;
      handler.on_packet(t, fh::Line::A, d, out);
    }
    REQUIRE(handler.core().counters().gaps == 0);
    REQUIRE(handler.core().counters().inconsistencies == 0);
    REQUIRE(handler.core().expected().value() == w.messages + 1);
  }
}

TEST_CASE("a workload can be made from a ground truth, keeping recorded bytes",
          "[bench][workload]") {
  fh::sim::GroundTruth truth = fh::sim::make_day(4, fh::sim::DayConfig{});
  truth.messages[20].raw = fh::sim::message_bytes(truth.messages[20]);  // as a recorded file would
  const Workload w = make_workload(truth, 4, 1);
  REQUIRE(w.messages == truth.messages.size());
  REQUIRE(w.datagrams.size() == truth.messages.size());
  REQUIRE(w.instruments == truth.instruments().size());
}
