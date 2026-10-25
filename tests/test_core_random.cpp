#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "consumer_model.hpp"
#include "fh/checksum.hpp"
#include "fh/core.hpp"
#include "mini_sim.hpp"

namespace {

struct Run {
  std::vector<fh::Output> outputs;
  fh::CoreCounters counters;
  std::optional<fh::FeedState> final_state;
  std::vector<std::string> failures;
  std::size_t updates{0};
};

Run run_scenario(const fhtest::Scenario& scenario, const fh::CoreConfig& config) {
  Run run;
  fh::Core core(config);
  fhtest::ConsumerModel consumer(scenario.truth);
  for (const fh::Input& input : scenario.inputs) {
    std::vector<fh::Output> out;
    core.step(input, out);
    for (const fh::Output& o : out) {
      consumer.consume(o);
      run.outputs.push_back(o);
    }
  }
  run.counters = core.counters();
  run.final_state = core.export_state();
  run.failures = consumer.failures();
  run.updates = consumer.updates_seen();
  return run;
}

// Faults hit the first part of each scenario only; afterwards a correct core must
// have repaired every book and agree with the oracle.
void require_correct(const fhtest::Scenario& scenario, const Run& run, std::uint64_t seed) {
  INFO("seed " << seed);
  for (const std::string& f : run.failures) UNSCOPED_INFO(f);
  REQUIRE(run.failures.empty());
  REQUIRE(run.final_state.has_value());
  const fh::FeedState expected = fhtest::oracle_state(scenario.truth, scenario.truth.size());
  REQUIRE(*run.final_state == expected);
  REQUIRE(fh::checksum(*run.final_state) == fh::checksum(expected));
}

}  // namespace

TEST_CASE("random faults: consumers see correct books and the core ends equal to the oracle",
          "[random]") {
  std::uint64_t gaps_seen = 0;
  std::uint64_t seeds_with_gaps = 0;
  for (std::uint64_t seed = 1; seed <= 300; ++seed) {
    fhtest::MiniSimConfig sim;
    sim.seed = seed;
    const fhtest::Scenario scenario = fhtest::make_scenario(sim);
    const Run run = run_scenario(scenario, fh::CoreConfig{});
    require_correct(scenario, run, seed);
    gaps_seen += run.counters.gaps;
    seeds_with_gaps += run.counters.gaps > 0 ? 1 : 0;
  }
  // The faults really did cause gaps; otherwise this test would prove little.
  REQUIRE(seeds_with_gaps > 30);
  REQUIRE(gaps_seen > 30);
}

TEST_CASE("random faults with tiny buffers still end correct", "[random]") {
  // Small limits force the overflow paths: reorder overflow, parked overflow.
  fh::CoreConfig config;
  config.sequencer.buffer_capacity = 3;
  config.arbiter_window = 8;
  config.instruments.parked_capacity = 3;
  std::uint64_t overflows = 0;
  for (std::uint64_t seed = 1; seed <= 200; ++seed) {
    fhtest::MiniSimConfig sim;
    sim.seed = 5000 + seed;
    sim.jitter = 400 * fh::kMicrosecond;  // more reordering
    const fhtest::Scenario scenario = fhtest::make_scenario(sim);
    const Run run = run_scenario(scenario, config);
    require_correct(scenario, run, sim.seed);
    overflows += run.counters.parked_overflows;
  }
  REQUIRE(overflows > 0);
}

TEST_CASE("the same inputs always give the same outputs and counters", "[random]") {
  for (std::uint64_t seed = 1; seed <= 20; ++seed) {
    fhtest::MiniSimConfig sim;
    sim.seed = seed;
    const fhtest::Scenario scenario = fhtest::make_scenario(sim);
    const Run first = run_scenario(scenario, fh::CoreConfig{});
    const Run second = run_scenario(scenario, fh::CoreConfig{});
    REQUIRE(first.outputs == second.outputs);
    REQUIRE(first.counters == second.counters);
    REQUIRE(first.final_state == second.final_state);
  }
}

TEST_CASE("extra copies of packets change nothing the consumer can see", "[random]") {
  // Property P7: delivering a packet a second time, right after the first, must
  // not change a single output.
  for (std::uint64_t seed = 1; seed <= 60; ++seed) {
    fhtest::MiniSimConfig sim;
    sim.seed = seed;
    const fhtest::Scenario scenario = fhtest::make_scenario(sim);

    fhtest::Scenario with_copies;
    with_copies.truth = scenario.truth;
    fhtest::SplitMix64 rng(seed);
    for (const fh::Input& input : scenario.inputs) {
      with_copies.inputs.push_back(input);
      const auto* packet = std::get_if<fh::PacketIn>(&input);
      if (packet != nullptr && packet->kind == fh::PacketKind::Messages && rng.below(100) < 30) {
        with_copies.inputs.push_back(input);
      }
    }

    const Run plain = run_scenario(scenario, fh::CoreConfig{});
    const Run copied = run_scenario(with_copies, fh::CoreConfig{});
    INFO("seed " << seed);
    REQUIRE(plain.outputs == copied.outputs);
    REQUIRE(copied.counters.duplicates + copied.counters.duplicates_unverified >=
            plain.counters.duplicates + plain.counters.duplicates_unverified);
  }
}
