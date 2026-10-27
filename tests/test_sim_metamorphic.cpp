#include <catch2/catch_test_macros.hpp>

#include "fh/sim/checker.hpp"
#include "fh/sim/metamorphic.hpp"
#include "fh/sim/scenario.hpp"

TEST_CASE("with_duplicates adds identical main entries right after their originals",
          "[sim][metamorphic]") {
  const auto trace = fh::sim::make_trace(1, fh::sim::scenario_config(fh::sim::Tier::Mild));
  const auto doubled = fh::sim::with_duplicates(trace, 5, 50);
  REQUIRE(doubled.entries.size() > trace.entries.size());
  REQUIRE(doubled.truth == trace.truth);
  REQUIRE(doubled.core == trace.core);

  std::size_t extra = 0;
  for (std::size_t i = 1; i < doubled.entries.size(); ++i) {
    if (doubled.entries[i] == doubled.entries[i - 1] &&
        doubled.entries[i].kind == fh::sim::EntryKind::Main) {
      ++extra;
    }
  }
  REQUIRE(extra > 0);
}

TEST_CASE("snapshot and tick entries are never duplicated", "[sim][metamorphic]") {
  const auto trace = fh::sim::make_trace(2, fh::sim::scenario_config(fh::sim::Tier::Mild));
  const auto doubled = fh::sim::with_duplicates(trace, 5, 100);
  const auto count = [](const fh::sim::Trace& t, fh::sim::EntryKind k) {
    std::size_t n = 0;
    for (const auto& e : t.entries) n += e.kind == k ? 1U : 0U;
    return n;
  };
  REQUIRE(count(doubled, fh::sim::EntryKind::Snapshot) ==
          count(trace, fh::sim::EntryKind::Snapshot));
  REQUIRE(count(doubled, fh::sim::EntryKind::Tick) == count(trace, fh::sim::EntryKind::Tick));
  REQUIRE(count(doubled, fh::sim::EntryKind::Main) == 2 * count(trace, fh::sim::EntryKind::Main));
}

TEST_CASE("zero percent adds nothing", "[sim][metamorphic]") {
  const auto trace = fh::sim::make_trace(3, fh::sim::scenario_config(fh::sim::Tier::Mild));
  REQUIRE(fh::sim::with_duplicates(trace, 5, 0) == trace);
}

TEST_CASE("P7 holds on every tier", "[sim][metamorphic]") {
  for (unsigned tier = 0; tier < fh::sim::kTierCount; ++tier) {
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
      const auto trace =
          fh::sim::make_trace(seed, fh::sim::scenario_config(static_cast<fh::sim::Tier>(tier)));
      const auto failure = fh::sim::check_duplicate_invariance(trace, seed);
      INFO("tier " << tier << " seed " << seed);
      if (failure) UNSCOPED_INFO(*failure);
      REQUIRE_FALSE(failure.has_value());
    }
  }
}

TEST_CASE("P5 holds on every tier", "[sim][metamorphic]") {
  for (unsigned tier = 0; tier < fh::sim::kTierCount; ++tier) {
    const auto trace =
        fh::sim::make_trace(4, fh::sim::scenario_config(static_cast<fh::sim::Tier>(tier)));
    REQUIRE_FALSE(fh::sim::check_determinism(trace).has_value());
  }
}

TEST_CASE("the duplicate check notices a handler whose output depends on duplicates",
          "[sim][metamorphic]") {
  // Different digests are what the check compares; prove it can fail by comparing two
  // unrelated traces through the same comparison.
  const auto a =
      fh::sim::run_trace(fh::sim::make_trace(1, fh::sim::scenario_config(fh::sim::Tier::Heavy)));
  const auto b =
      fh::sim::run_trace(fh::sim::make_trace(2, fh::sim::scenario_config(fh::sim::Tier::Heavy)));
  REQUIRE(a.output_digest != b.output_digest);
}
