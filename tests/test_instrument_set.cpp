#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "fh/instrument_set.hpp"
#include "packets.hpp"

using fh::InstrumentHealth;
using fh::Side;

namespace {

fh::InstrumentConfig config() { return fh::InstrumentConfig{}; }

int health_changes_for(const std::vector<fh::Output>& out, fh::InstrumentId inst,
                       InstrumentHealth state) {
  int n = 0;
  for (const fh::Output& o : out) {
    if (const auto* h = std::get_if<fh::InstrumentHealthChange>(&o)) {
      n += (h->inst == inst && h->state == state) ? 1 : 0;
    }
  }
  return n;
}

}  // namespace

TEST_CASE("an instrument first seen when nothing is missing is created live", "[instruments]") {
  fh::InstrumentSet set(config());
  std::vector<fh::Output> out;
  set.set_initial_need(0, out);
  (void)set.on_event(fh::make_add(1, 7, 101, Side::Buy, 100, 10), out);
  REQUIRE(set.states().size() == 1);
  REQUIRE(set.states().at(7).health() == InstrumentHealth::Live);
  REQUIRE(health_changes_for(out, 7, InstrumentHealth::Live) == 1);
}

TEST_CASE("an instrument first seen after joining mid-stream starts recovering", "[instruments]") {
  fh::InstrumentSet set(config());
  std::vector<fh::Output> out;
  set.set_initial_need(5, out);  // we joined at sequence 6
  (void)set.on_event(fh::make_add(6, 7, 101, Side::Buy, 100, 10), out);
  REQUIRE(set.states().at(7).health() == InstrumentHealth::Syncing);
  REQUIRE(set.states().at(7).need() == 5);
  REQUIRE(set.states().at(7).pending() == 1);
}

TEST_CASE("a gap hits every known instrument and instruments created later", "[instruments]") {
  fh::InstrumentSet set(config());
  std::vector<fh::Output> out;
  set.set_initial_need(0, out);
  (void)set.on_event(fh::make_add(1, 1, 101, Side::Buy, 100, 10), out);
  (void)set.on_event(fh::make_add(2, 2, 201, Side::Buy, 100, 10), out);
  out.clear();

  set.on_gap(5, out);
  REQUIRE(set.global_need() == 5);
  REQUIRE(set.states().at(1).health() == InstrumentHealth::Syncing);
  REQUIRE(set.states().at(2).health() == InstrumentHealth::Syncing);
  REQUIRE(health_changes_for(out, 1, InstrumentHealth::Syncing) == 1);
  REQUIRE(health_changes_for(out, 2, InstrumentHealth::Syncing) == 1);
  REQUIRE_FALSE(set.all_live());

  (void)set.on_event(fh::make_add(6, 3, 301, Side::Buy, 100, 10), out);  // a new instrument
  REQUIRE(set.states().at(3).health() == InstrumentHealth::Syncing);
  REQUIRE(set.states().at(3).need() == 5);
}

TEST_CASE("a snapshot for an unknown instrument creates and repairs it", "[instruments]") {
  fh::InstrumentSet set(config());
  std::vector<fh::Output> out;
  set.set_initial_need(5, out);
  const fh::SnapshotRun run{9, 6, {{1, fh::Order{Side::Buy, 100, 10}}}};
  REQUIRE(set.on_snapshot(run, out) == fh::SnapshotOutcome::Applied);
  REQUIRE(set.states().at(9).health() == InstrumentHealth::Live);
  REQUIRE(set.all_live());
}

TEST_CASE("an empty set counts as all live", "[instruments]") {
  fh::InstrumentSet set(config());
  REQUIRE(set.all_live());
}

TEST_CASE("an instrument that exists before the feed is first seen starts recovering",
          "[instruments]") {
  // A snapshot can arrive before any main-feed packet. It creates the instrument live
  // with an empty book, which is only right if the feed starts at sequence 1.
  fh::InstrumentSet set(config());
  std::vector<fh::Output> out;
  const fh::SnapshotRun run{7, 0, {}};
  REQUIRE(set.on_snapshot(run, out) == fh::SnapshotOutcome::Ignored);
  REQUIRE(set.states().at(7).health() == InstrumentHealth::Live);

  out.clear();
  set.set_initial_need(10, out);  // the first packet we saw starts at sequence 11
  REQUIRE(set.states().at(7).health() == InstrumentHealth::Syncing);
  REQUIRE(set.states().at(7).need() == 10);
  REQUIRE(health_changes_for(out, 7, InstrumentHealth::Syncing) == 1);
}

TEST_CASE("a feed that starts at sequence 1 leaves existing instruments live", "[instruments]") {
  fh::InstrumentSet set(config());
  std::vector<fh::Output> out;
  (void)set.on_snapshot(fh::SnapshotRun{7, 0, {}}, out);
  out.clear();
  set.set_initial_need(0, out);
  REQUIRE(set.states().at(7).health() == InstrumentHealth::Live);
  REQUIRE(out.empty());
}
