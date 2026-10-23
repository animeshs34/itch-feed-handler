#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <vector>

#include "fh/instrument_state.hpp"
#include "packets.hpp"

using fh::AlarmKind;
using fh::EventOutcome;
using fh::InstrumentHealth;
using fh::Order;
using fh::Side;
using fh::SnapshotOutcome;
using fh::UpdateKind;

namespace {

constexpr fh::InstrumentId kInst = 1;

fh::InstrumentConfig config(std::size_t parked = 100) {
  fh::InstrumentConfig c;
  c.parked_capacity = parked;
  return c;
}

fh::SnapshotRun run(fh::Seq asof, std::map<fh::OrderId, Order> orders) {
  return fh::SnapshotRun{kInst, asof, std::move(orders)};
}

std::vector<fh::Update> updates(const std::vector<fh::Output>& out) {
  std::vector<fh::Update> result;
  for (const fh::Output& o : out) {
    if (const auto* u = std::get_if<fh::Update>(&o)) result.push_back(*u);
  }
  return result;
}

int alarms(const std::vector<fh::Output>& out, AlarmKind kind) {
  int n = 0;
  for (const fh::Output& o : out) {
    if (const auto* a = std::get_if<fh::Alarm>(&o)) n += a->kind == kind ? 1 : 0;
  }
  return n;
}

int health_changes(const std::vector<fh::Output>& out, InstrumentHealth state) {
  int n = 0;
  for (const fh::Output& o : out) {
    if (const auto* h = std::get_if<fh::InstrumentHealthChange>(&o)) n += h->state == state ? 1 : 0;
  }
  return n;
}

}  // namespace

TEST_CASE("an instrument with nothing missing starts live and an empty book", "[instrument]") {
  const fh::InstrumentState s(kInst, 0, config());
  REQUIRE(s.health() == InstrumentHealth::Live);
  REQUIRE(s.book().orders().empty());
  REQUIRE(s.last_seq() == 0);
  REQUIRE(s.epoch() == 0);
}

TEST_CASE("an instrument with something missing starts recovering", "[instrument]") {
  const fh::InstrumentState s(kInst, 5, config());
  REQUIRE(s.health() == InstrumentHealth::Syncing);
  REQUIRE(s.need() == 5);
}

TEST_CASE("a live instrument applies events and publishes a chain of updates", "[instrument]") {
  fh::InstrumentState s(kInst, 0, config());
  std::vector<fh::Output> out;
  REQUIRE(s.on_event(fh::make_add(3, kInst, 101, Side::Buy, 100, 10), out) ==
          EventOutcome::Applied);
  REQUIRE(s.on_event(fh::make_reduce(7, kInst, 101, 4), out) == EventOutcome::Applied);

  const auto u = updates(out);
  REQUIRE(u.size() == 2);
  REQUIRE(u[0].kind == UpdateKind::Delta);
  REQUIRE(u[0].seq == 3);
  REQUIRE(u[0].prev == 0);  // first update of the epoch points back at 0
  REQUIRE(u[0].epoch == 0);
  REQUIRE(u[0].level_count == 1);
  REQUIRE(u[0].levels[0].total_after == 10);
  REQUIRE(u[1].seq == 7);
  REQUIRE(u[1].prev == 3);  // the chain: this update follows seq 3 for this instrument
  REQUIRE(u[1].levels[0].total_after == 6);
  REQUIRE(s.last_seq() == 7);
}

TEST_CASE("an event already inside the book is skipped as covered", "[instrument]") {
  fh::InstrumentState s(kInst, 0, config());
  std::vector<fh::Output> out;
  REQUIRE(s.on_event(fh::make_add(5, kInst, 101, Side::Buy, 100, 10), out) ==
          EventOutcome::Applied);
  out.clear();
  REQUIRE(s.on_event(fh::make_add(5, kInst, 102, Side::Buy, 100, 10), out) ==
          EventOutcome::Covered);
  REQUIRE(s.on_event(fh::make_add(4, kInst, 103, Side::Buy, 100, 10), out) ==
          EventOutcome::Covered);
  REQUIRE(out.empty());
}

TEST_CASE("a gap makes a live instrument recover and says so once", "[instrument]") {
  fh::InstrumentState s(kInst, 0, config());
  std::vector<fh::Output> out;
  s.on_gap(5, out);
  REQUIRE(s.health() == InstrumentHealth::Syncing);
  REQUIRE(s.need() == 5);
  REQUIRE(health_changes(out, InstrumentHealth::Syncing) == 1);

  s.on_gap(9, out);  // already recovering: only the need moves
  REQUIRE(s.need() == 9);
  REQUIRE(health_changes(out, InstrumentHealth::Syncing) == 1);
}

TEST_CASE("a recovering instrument parks events and publishes nothing", "[instrument]") {
  fh::InstrumentState s(kInst, 5, config());
  std::vector<fh::Output> out;
  REQUIRE(s.on_event(fh::make_add(6, kInst, 101, Side::Buy, 100, 10), out) == EventOutcome::Parked);
  REQUIRE(s.pending() == 1);
  REQUIRE(out.empty());
}

TEST_CASE("a snapshot older than the need is stale", "[instrument]") {
  fh::InstrumentState s(kInst, 5, config());
  std::vector<fh::Output> out;
  REQUIRE(s.on_snapshot(run(4, {}), out) == SnapshotOutcome::Stale);
  REQUIRE(s.health() == InstrumentHealth::Syncing);
  REQUIRE(out.empty());
}

TEST_CASE("a usable snapshot resets the book then replays what was parked after it",
          "[instrument]") {
  fh::InstrumentState s(kInst, 5, config());
  std::vector<fh::Output> out;
  (void)s.on_event(fh::make_add(6, kInst, 201, Side::Sell, 50, 7),
                   out);  // before the snapshot as-of
  (void)s.on_event(fh::make_add(8, kInst, 202, Side::Sell, 50, 4), out);  // after it
  (void)s.on_event(fh::make_reduce(9, kInst, 202, 1), out);               // after it
  REQUIRE(out.empty());

  const std::map<fh::OrderId, Order> orders = {{201, Order{Side::Sell, 50, 7}}};
  REQUIRE(s.on_snapshot(run(7, orders), out) == SnapshotOutcome::Applied);

  REQUIRE(s.health() == InstrumentHealth::Live);
  REQUIRE(s.epoch() == 1);
  REQUIRE(s.last_seq() == 9);
  REQUIRE(s.pending() == 0);
  REQUIRE(health_changes(out, InstrumentHealth::Live) == 1);

  const auto u = updates(out);
  REQUIRE(u.size() == 3);
  REQUIRE(u[0].kind == UpdateKind::Reset);
  REQUIRE(u[0].seq == 7);
  REQUIRE_FALSE(u[0].prev.has_value());
  REQUIRE(u[0].epoch == 1);
  REQUIRE(u[0].orders == orders);
  REQUIRE(u[1].seq == 8);
  REQUIRE(u[1].prev == 7);  // the chain restarts at the snapshot's as-of
  REQUIRE(u[2].seq == 9);
  REQUIRE(u[2].prev == 8);
  REQUIRE(s.book().orders().at(202).shares == 3);
}

TEST_CASE("a snapshot ahead of the feed makes later older events covered", "[instrument]") {
  fh::InstrumentState s(kInst, 5, config());
  std::vector<fh::Output> out;
  REQUIRE(s.on_snapshot(run(10, {}), out) == SnapshotOutcome::Applied);
  REQUIRE(s.last_seq() == 10);
  REQUIRE(s.on_event(fh::make_add(9, kInst, 1, Side::Buy, 1, 1), out) == EventOutcome::Covered);
  REQUIRE(s.on_event(fh::make_add(10, kInst, 1, Side::Buy, 1, 1), out) == EventOutcome::Covered);
  REQUIRE(s.on_event(fh::make_add(11, kInst, 1, Side::Buy, 1, 1), out) == EventOutcome::Applied);
}

TEST_CASE("a snapshot for a live instrument is ignored", "[instrument]") {
  fh::InstrumentState s(kInst, 0, config());
  std::vector<fh::Output> out;
  REQUIRE(s.on_snapshot(run(10, {}), out) == SnapshotOutcome::Ignored);
  REQUIRE(out.empty());
  REQUIRE(s.epoch() == 0);
}

TEST_CASE("an event the book cannot apply sends a live instrument back to recovery",
          "[instrument]") {
  fh::InstrumentState s(kInst, 0, config());
  std::vector<fh::Output> out;
  REQUIRE(s.on_event(fh::make_reduce(4, kInst, 999, 1), out) == EventOutcome::Inconsistent);
  REQUIRE(s.health() == InstrumentHealth::Syncing);
  REQUIRE(s.need() == 4);
  REQUIRE(s.inconsistencies() == 1);
  REQUIRE(alarms(out, AlarmKind::BookInconsistent) == 1);
  REQUIRE(health_changes(out, InstrumentHealth::Syncing) == 1);
  REQUIRE(updates(out).empty());
}

TEST_CASE("an inconsistent event found while replaying leaves the instrument recovering",
          "[instrument]") {
  fh::InstrumentState s(kInst, 5, config());
  std::vector<fh::Output> out;
  (void)s.on_event(fh::make_reduce(8, kInst, 999, 1), out);  // refers to an order nobody has
  (void)s.on_event(fh::make_add(9, kInst, 1, Side::Buy, 1, 1), out);
  REQUIRE(s.on_snapshot(run(7, {}), out) == SnapshotOutcome::Applied);
  REQUIRE(s.health() == InstrumentHealth::Syncing);
  REQUIRE(s.need() == 8);
  REQUIRE(s.pending() == 0);
  REQUIRE(alarms(out, AlarmKind::BookInconsistent) == 1);
}

TEST_CASE("too many parked events are dropped and a covering snapshot is required",
          "[instrument]") {
  fh::InstrumentState s(kInst, 1, config(/*parked=*/2));
  std::vector<fh::Output> out;
  REQUIRE(s.on_event(fh::make_add(2, kInst, 1, Side::Buy, 1, 1), out) == EventOutcome::Parked);
  REQUIRE(s.on_event(fh::make_add(3, kInst, 2, Side::Buy, 1, 1), out) == EventOutcome::Parked);
  REQUIRE(s.on_event(fh::make_add(4, kInst, 3, Side::Buy, 1, 1), out) ==
          EventOutcome::ParkedOverflow);
  REQUIRE(s.pending() == 0);
  REQUIRE(s.need() == 4);
  REQUIRE(alarms(out, AlarmKind::ParkedOverflow) == 1);
  REQUIRE(s.on_snapshot(run(3, {}), out) == SnapshotOutcome::Stale);  // 3 does not cover 4
}

TEST_CASE("a snapshot whose orders overflow a level is rejected", "[instrument]") {
  constexpr fh::Shares kMax = std::numeric_limits<fh::Shares>::max();
  fh::InstrumentState s(kInst, 1, config());
  std::vector<fh::Output> out;
  const std::map<fh::OrderId, Order> bad = {{1, Order{Side::Buy, 100, kMax}},
                                            {2, Order{Side::Buy, 100, 1}}};
  REQUIRE(s.on_snapshot(run(5, bad), out) == SnapshotOutcome::Rejected);
  REQUIRE(s.health() == InstrumentHealth::Syncing);
  REQUIRE(alarms(out, AlarmKind::SnapshotRejected) == 1);
}
