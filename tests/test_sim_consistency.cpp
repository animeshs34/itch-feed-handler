#include <catch2/catch_test_macros.hpp>

#include "fh/sim/consistency.hpp"
#include "fh/sim/day.hpp"
#include "oracle_book.hpp"
#include "prng.hpp"

namespace {

fh::sim::GroundTruth truth_of(std::vector<fh::BookEvent> events) {
  fh::sim::GroundTruth t;
  for (auto& e : events) {
    fh::sim::GroundMessage m;
    m.seq = t.messages.size() + 1;
    e.seq = m.seq;
    m.event = e;
    t.messages.push_back(m);
  }
  return t;
}

}  // namespace

TEST_CASE("a synthetic day is consistent", "[sim][consistency]") {
  for (std::uint64_t seed = 1; seed <= 100; ++seed) {
    const auto report = fh::sim::check_consistency(fh::sim::make_day(seed, fh::sim::DayConfig{}));
    INFO("seed " << seed);
    REQUIRE(report.ok());
    REQUIRE(report.events_checked == 400);
  }
}

TEST_CASE("each rule of a valid stream is checked", "[sim][consistency]") {
  using fh::Side;
  const auto add = [](fh::OrderId id, fh::Shares n) {
    return fh::make_add(0, 1, id, Side::Buy, 100, n);
  };
  // reduce of an unknown order
  REQUIRE(fh::sim::check_consistency(truth_of({fh::make_reduce(0, 1, 9, 1)})).violations == 1);
  // reduce by more than the order has
  REQUIRE(
      fh::sim::check_consistency(truth_of({add(1, 5), fh::make_reduce(0, 1, 1, 6)})).violations ==
      1);
  // delete of an unknown order, and a second delete of the same order
  REQUIRE(fh::sim::check_consistency(truth_of({fh::make_delete(0, 1, 9)})).violations == 1);
  REQUIRE(fh::sim::check_consistency(
              truth_of({add(1, 5), fh::make_delete(0, 1, 1), fh::make_delete(0, 1, 1)}))
              .violations == 1);
  // an add that reuses a live reference
  REQUIRE(fh::sim::check_consistency(truth_of({add(1, 5), add(1, 7)})).violations == 1);
  // replace of an unknown order, and a replace whose new reference is live
  REQUIRE(
      fh::sim::check_consistency(truth_of({fh::make_replace(0, 1, 9, 10, 100, 5)})).violations ==
      1);
  REQUIRE(fh::sim::check_consistency(
              truth_of({add(1, 5), add(2, 5), fh::make_replace(0, 1, 1, 2, 100, 5)}))
              .violations == 1);
  // zero shares
  REQUIRE(fh::sim::check_consistency(truth_of({add(1, 0)})).violations == 1);
  // a valid reference may be reused once the order is gone
  REQUIRE(
      fh::sim::check_consistency(truth_of({add(1, 5), fh::make_delete(0, 1, 1), add(1, 5)})).ok());
  // a full reduce kills the order
  REQUIRE(fh::sim::check_consistency(
              truth_of({add(1, 5), fh::make_reduce(0, 1, 1, 5), fh::make_reduce(0, 1, 1, 1)}))
              .violations == 1);
}

TEST_CASE("the report lists the first violations in order and counts them all",
          "[sim][consistency]") {
  std::vector<fh::BookEvent> events;
  for (int i = 0; i < 50; ++i)
    events.push_back(fh::make_delete(0, 1, 1000 + static_cast<fh::OrderId>(i)));
  const auto report = fh::sim::check_consistency(truth_of(events));
  REQUIRE(report.violations == 50);
  REQUIRE(report.first.size() == fh::sim::kMaxListedViolations);
  REQUIRE(report.first[0].seq == 1);
  REQUIRE(report.first[0].order == 1000);
  REQUIRE(report.first[0].kind == fh::EventKind::Delete);
}

TEST_CASE("a day sliced from the middle is inconsistent", "[sim][consistency]") {
  auto day = fh::sim::make_day(2, fh::sim::DayConfig{});
  day.messages.erase(day.messages.begin(), day.messages.begin() + 150);
  for (std::size_t i = 0; i < day.messages.size(); ++i) {
    day.messages[i].seq = i + 1;
    if (day.messages[i].event) day.messages[i].event->seq = i + 1;
  }
  const auto report = fh::sim::check_consistency(day);
  REQUIRE(report.violations > 0);
  REQUIRE(report.numbering_ok);
}

TEST_CASE("wrong numbering is reported", "[sim][consistency]") {
  auto day = fh::sim::make_day(3, fh::sim::DayConfig{});
  day.messages[5].seq = 99;
  REQUIRE_FALSE(fh::sim::check_consistency(day).numbering_ok);
}

TEST_CASE("it agrees with the oracle on random streams, valid and not", "[sim][consistency]") {
  fhtest::SplitMix64 rng(7);
  for (int round = 0; round < 200; ++round) {
    std::vector<fh::BookEvent> events;
    for (int i = 0; i < 60; ++i) {
      const auto id = static_cast<fh::OrderId>(1 + rng.below(12));
      const auto n = static_cast<fh::Shares>(rng.below(5));  // zero is possible on purpose
      switch (rng.below(4)) {
        case 0:
          events.push_back(fh::make_add(0, 1, id, fh::Side::Buy, 100, n));
          break;
        case 1:
          events.push_back(fh::make_reduce(0, 1, id, n));
          break;
        case 2:
          events.push_back(fh::make_delete(0, 1, id));
          break;
        default:
          events.push_back(
              fh::make_replace(0, 1, id, static_cast<fh::OrderId>(1 + rng.below(12)), 100, n));
          break;
      }
    }
    oracle::Book book;
    std::uint64_t expected = 0;
    for (const auto& e : events) expected += book.apply(e) ? 0U : 1U;
    INFO("round " << round);
    REQUIRE(fh::sim::check_consistency(truth_of(events)).violations == expected);
  }
}
