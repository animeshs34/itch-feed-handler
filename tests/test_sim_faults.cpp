#include <catch2/catch_test_macros.hpp>

#include "fh/sim/faults.hpp"

TEST_CASE("the clean tier injects no fault at all", "[sim][faults]") {
  const fh::sim::FaultConfig c = fh::sim::tier_config(fh::sim::Tier::Clean);
  REQUIRE(c.loss_pct == 0);
  REQUIRE(c.burst_pct == 0);
  REQUIRE(c.dup_pct == 0);
  REQUIRE(c.mismatch_pct == 0);
  REQUIRE(c.malformed_pct == 0);
  REQUIRE(c.wrong_session_pct == 0);
  REQUIRE(c.silence_windows == 0);
  REQUIRE(c.end_loss_pct == 0);
  REQUIRE(c.rate_burst_permille == 0);
  REQUIRE(c.snap_loss_pct == 0);
  REQUIRE(c.snap_split_loss_pct == 0);
  REQUIRE(c.snap_late_pct == 0);
  REQUIRE(c.snap_dup_pct == 0);
}

TEST_CASE("each tier is at least as harsh as the one before", "[sim][faults]") {
  for (unsigned i = 1; i < fh::sim::kTierCount; ++i) {
    const auto lo = fh::sim::tier_config(static_cast<fh::sim::Tier>(i - 1));
    const auto hi = fh::sim::tier_config(static_cast<fh::sim::Tier>(i));
    INFO("tier " << i);
    REQUIRE(hi.loss_pct >= lo.loss_pct);
    REQUIRE(hi.burst_pct >= lo.burst_pct);
    REQUIRE(hi.dup_pct >= lo.dup_pct);
    REQUIRE(hi.mismatch_pct >= lo.mismatch_pct);
    REQUIRE(hi.malformed_pct >= lo.malformed_pct);
    REQUIRE(hi.silence_windows >= lo.silence_windows);
    REQUIRE(hi.end_loss_pct >= lo.end_loss_pct);
    REQUIRE(hi.jitter >= lo.jitter);
    REQUIRE(hi.snap_loss_pct >= lo.snap_loss_pct);
    REQUIRE(hi.faulty_pct >= lo.faulty_pct);
  }
}

TEST_CASE("every tier leaves part of the day clean", "[sim][faults]") {
  for (unsigned i = 0; i < fh::sim::kTierCount; ++i) {
    REQUIRE(fh::sim::tier_config(static_cast<fh::sim::Tier>(i)).faulty_pct < 100);
  }
}

TEST_CASE("tier names are distinct", "[sim][faults]") {
  REQUIRE(fh::sim::tier_name(fh::sim::Tier::Clean) == "clean");
  REQUIRE(fh::sim::tier_name(fh::sim::Tier::Brutal) == "brutal");
  REQUIRE(fh::sim::tier_name(fh::sim::Tier::Mild) != fh::sim::tier_name(fh::sim::Tier::Heavy));
}

TEST_CASE("core params map onto the core's own config", "[sim][faults]") {
  fh::sim::CoreParams p;
  p.skew_window = 2 * fh::kMillisecond;
  p.silent_window = 9 * fh::kMillisecond;
  p.buffer_capacity = 7;
  p.arbiter_window = 11;
  p.parked_capacity = 13;
  const fh::CoreConfig c = fh::sim::to_core_config(p);
  REQUIRE(c.sequencer.skew_window == 2 * fh::kMillisecond);
  REQUIRE(c.lines.silent_window == 9 * fh::kMillisecond);
  REQUIRE(c.sequencer.buffer_capacity == 7);
  REQUIRE(c.arbiter_window == 11);
  REQUIRE(c.instruments.parked_capacity == 13);
}

TEST_CASE("the brutal tier uses buffers small enough to overflow", "[sim][faults]") {
  const auto c = fh::sim::tier_config(fh::sim::Tier::Brutal);
  REQUIRE(c.core.buffer_capacity < 10);
  REQUIRE(c.core.parked_capacity < 10);
  REQUIRE(fh::sim::tier_config(fh::sim::Tier::Mild).core == fh::sim::CoreParams{});
}
