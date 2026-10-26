#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "fh/core.hpp"
#include "fh/nasdaq/handler.hpp"
#include "input_to_bytes.hpp"
#include "mini_sim.hpp"

namespace {

std::vector<fh::Output> run_decoded(const fhtest::Scenario& scenario, const fh::CoreConfig& cfg,
                                    std::optional<fh::FeedState>& final_state) {
  fh::Core core(cfg);
  std::vector<fh::Output> all;
  for (const fh::Input& input : scenario.inputs) core.step(input, all);
  final_state = core.export_state();
  return all;
}

std::vector<fh::Output> run_bytes(const fhtest::Scenario& scenario, const fh::CoreConfig& cfg,
                                  std::optional<fh::FeedState>& final_state) {
  const auto main_session = fh::nasdaq::make_session("MAINSESS01");
  const auto snap_session = fh::nasdaq::make_session("SNAPSESS01");
  fh::nasdaq::Handler handler(cfg);
  std::vector<fh::Output> all;
  for (const fh::Input& input : scenario.inputs) {
    const fhtest::ByteStep step = fhtest::to_byte_step(input, main_session, snap_session);
    switch (step.kind) {
      case fhtest::ByteStep::Kind::Main:
        handler.on_packet(step.t, step.line, step.bytes, all);
        break;
      case fhtest::ByteStep::Kind::Snapshot:
        handler.on_snapshot_packet(step.t, step.bytes, all);
        break;
      case fhtest::ByteStep::Kind::Tick:
        handler.on_tick(step.t, all);
        break;
    }
  }
  final_state = handler.core().export_state();
  return all;
}

}  // namespace

TEST_CASE("going through bytes changes nothing: the handler equals the core on decoded input",
          "[nasdaq][random]") {
  std::uint64_t gaps = 0;
  for (std::uint64_t seed = 1; seed <= 100; ++seed) {
    fhtest::MiniSimConfig sim;
    sim.seed = seed;
    const fhtest::Scenario scenario = fhtest::make_scenario(sim);

    std::optional<fh::FeedState> decoded_state, byte_state;
    const auto decoded = run_decoded(scenario, fh::CoreConfig{}, decoded_state);
    const auto bytes = run_bytes(scenario, fh::CoreConfig{}, byte_state);

    INFO("seed " << seed);
    REQUIRE(decoded.size() == bytes.size());
    REQUIRE(decoded == bytes);
    REQUIRE(decoded_state == byte_state);
    REQUIRE(byte_state.has_value());
    REQUIRE(*byte_state == fhtest::oracle_state(scenario.truth, scenario.truth.size()));
    for (const fh::Output& o : bytes) gaps += std::holds_alternative<fh::GapDeclared>(o) ? 1U : 0U;
  }
  REQUIRE(gaps > 30);  // the scenarios really did lose messages
}

TEST_CASE("the same holds with tiny buffers that force the overflow paths", "[nasdaq][random]") {
  fh::CoreConfig config;
  config.sequencer.buffer_capacity = 3;
  config.arbiter_window = 8;
  config.instruments.parked_capacity = 3;
  for (std::uint64_t seed = 1; seed <= 50; ++seed) {
    fhtest::MiniSimConfig sim;
    sim.seed = 9000 + seed;
    sim.jitter = 400 * fh::kMicrosecond;
    const fhtest::Scenario scenario = fhtest::make_scenario(sim);
    std::optional<fh::FeedState> a, b;
    INFO("seed " << sim.seed);
    REQUIRE(run_decoded(scenario, config, a) == run_bytes(scenario, config, b));
    REQUIRE(a == b);
  }
}
