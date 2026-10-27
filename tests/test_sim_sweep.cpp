#include <catch2/catch_test_macros.hpp>

#include "fh/sim/checker.hpp"
#include "fh/sim/scenario.hpp"

// The first sweep over every tier. When it was first run, tier 4 (brutal) seed 14 failed
// with "P9: BookInconsistent alarm": see test_core_late_join.cpp for what was wrong.
TEST_CASE("every tier ends with correct books on a spread of seeds", "[sim][checker]") {
  for (unsigned tier = 0; tier < fh::sim::kTierCount; ++tier) {
    for (std::uint64_t seed = 1; seed <= 15; ++seed) {
      const auto trace =
          fh::sim::make_trace(seed, fh::sim::scenario_config(static_cast<fh::sim::Tier>(tier)));
      const auto verdict = fh::sim::run_trace(trace);
      INFO("tier " << tier << " seed " << seed);
      for (const auto& f : verdict.safety_failures) UNSCOPED_INFO(f);
      for (const auto& f : verdict.liveness_failures) UNSCOPED_INFO(f);
      REQUIRE(verdict.ok());
    }
  }
}

TEST_CASE("a wider sweep over every tier stays correct", "[sim][sweep]") {
  for (unsigned tier = 0; tier < fh::sim::kTierCount; ++tier) {
    for (std::uint64_t seed = 100; seed < 400; ++seed) {
      const auto verdict = fh::sim::run_trace(
          fh::sim::make_trace(seed, fh::sim::scenario_config(static_cast<fh::sim::Tier>(tier))));
      INFO("tier " << tier << " seed " << seed);
      for (const auto& f : verdict.safety_failures) UNSCOPED_INFO(f);
      for (const auto& f : verdict.liveness_failures) UNSCOPED_INFO(f);
      REQUIRE(verdict.ok());
    }
  }
}
