#include <catch2/catch_test_macros.hpp>

#include "fh/sim/checker.hpp"
#include "fh/sim/scenario.hpp"

TEST_CASE("a clean day passes every property", "[sim][checker]") {
  for (std::uint64_t seed = 1; seed <= 20; ++seed) {
    const auto trace = fh::sim::make_trace(seed, fh::sim::scenario_config(fh::sim::Tier::Clean));
    const auto verdict = fh::sim::run_trace(trace);
    INFO("seed " << seed);
    for (const auto& f : verdict.safety_failures) UNSCOPED_INFO(f);
    for (const auto& f : verdict.liveness_failures) UNSCOPED_INFO(f);
    REQUIRE(verdict.ok());
    REQUIRE(verdict.updates > 100);
    REQUIRE(verdict.final_checksum != 0);
    REQUIRE(verdict.counters.gaps == 0);
  }
}

TEST_CASE("the faults really cause gaps, mismatches and snapshot recoveries", "[sim][checker]") {
  std::uint64_t gaps = 0, mismatches = 0, resets = 0, malformed = 0;
  for (std::uint64_t seed = 1; seed <= 20; ++seed) {
    const auto verdict = fh::sim::run_trace(
        fh::sim::make_trace(seed, fh::sim::scenario_config(fh::sim::Tier::Heavy)));
    gaps += verdict.counters.gaps;
    mismatches += verdict.counters.line_mismatches;
    malformed +=
        verdict.packet_stats
            .packet_errors[static_cast<std::size_t>(fh::nasdaq::PacketError::BlockOverrun)];
    resets += verdict.output_counts[0];  // Update alternative; resets are a subset
  }
  REQUIRE(gaps > 20);
  REQUIRE(mismatches > 0);
  REQUIRE(malformed > 0);
  REQUIRE(resets > 0);
}

TEST_CASE("a trace with all snapshots removed and messages lost fails P4 but stays safe",
          "[sim][checker]") {
  auto trace = fh::sim::make_trace(5, fh::sim::scenario_config(fh::sim::Tier::Moderate));
  // Remove every snapshot datagram and every main datagram of the middle of the day.
  std::vector<fh::sim::Entry> kept;
  const fh::Nanos mid = trace.entries.back().t / 3;
  for (const auto& e : trace.entries) {
    if (e.kind == fh::sim::EntryKind::Snapshot) continue;
    if (e.kind == fh::sim::EntryKind::Main && e.t > mid && e.t < 2 * mid) continue;
    kept.push_back(e);
  }
  trace.entries = kept;
  const auto verdict = fh::sim::run_trace(trace);
  REQUIRE(verdict.safe());
  REQUIRE_FALSE(verdict.liveness_failures.empty());
  REQUIRE(verdict.liveness_failures.front().starts_with("P4"));
}

TEST_CASE("replaying a trace twice gives the same fingerprint and checksum", "[sim][checker]") {
  const auto trace = fh::sim::make_trace(9, fh::sim::scenario_config(fh::sim::Tier::Heavy));
  const auto a = fh::sim::run_trace(trace);
  const auto b = fh::sim::run_trace(trace);
  REQUIRE(a.output_digest == b.output_digest);
  REQUIRE(a.final_checksum == b.final_checksum);
  REQUIRE(a.counters == b.counters);
}
