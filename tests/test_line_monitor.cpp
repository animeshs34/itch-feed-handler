#include <catch2/catch_test_macros.hpp>

#include "fh/line_monitor.hpp"

using fh::FeedHealth;
using fh::Line;

namespace {
constexpr fh::Nanos kUnit = fh::kMillisecond;

fh::LineMonitor monitor() {
  fh::LineMonitorConfig c;
  c.silent_window = 7 * kUnit;
  return fh::LineMonitor(c);
}
}  // namespace

TEST_CASE("both lines start up and the feed is live", "[lines]") {
  const fh::LineMonitor m = monitor();
  REQUIRE(m.health() == FeedHealth::Live);
  REQUIRE(m.line_up(Line::A));
  REQUIRE(m.line_up(Line::B));
}

TEST_CASE("hearing a line does not change a healthy feed", "[lines]") {
  fh::LineMonitor m = monitor();
  REQUIRE_FALSE(m.heard(Line::A, 1 * kUnit).has_value());
  REQUIRE_FALSE(m.tick(2 * kUnit).has_value());
}

TEST_CASE("one silent line degrades the feed and two make it stale", "[lines]") {
  fh::LineMonitor m = monitor();
  (void)m.heard(Line::A, 1 * kUnit);  // the clock starts here for both lines
  (void)m.heard(Line::B, 1 * kUnit);
  (void)m.heard(Line::A, 7 * kUnit);
  REQUIRE_FALSE(m.tick(7 * kUnit).has_value());        // B silent for 6 units
  REQUIRE(m.tick(8 * kUnit) == FeedHealth::Degraded);  // B silent for 7 units
  REQUIRE_FALSE(m.line_up(Line::B));
  REQUIRE(m.line_up(Line::A));
  REQUIRE_FALSE(m.tick(9 * kUnit).has_value());  // unchanged

  REQUIRE(m.tick(14 * kUnit) == FeedHealth::Stale);  // A silent since 7
  REQUIRE_FALSE(m.line_up(Line::A));
}

TEST_CASE("a line that speaks again comes back up", "[lines]") {
  fh::LineMonitor m = monitor();
  (void)m.heard(Line::A, 0);
  (void)m.heard(Line::B, 0);
  REQUIRE(m.tick(10 * kUnit) == FeedHealth::Stale);
  REQUIRE(m.heard(Line::B, 11 * kUnit) == FeedHealth::Degraded);
  REQUIRE(m.heard(Line::A, 12 * kUnit) == FeedHealth::Live);
  REQUIRE(m.line_up(Line::A));
  REQUIRE(m.line_up(Line::B));
}

TEST_CASE("the end of the session ends the feed and silence is then expected", "[lines]") {
  fh::LineMonitor m = monitor();
  (void)m.heard(Line::A, 0);
  REQUIRE(m.end_of_session() == FeedHealth::Ended);
  REQUIRE_FALSE(m.tick(1000 * kUnit).has_value());
  REQUIRE(m.health() == FeedHealth::Ended);
  REQUIRE_FALSE(m.heard(Line::A, 1001 * kUnit).has_value());
  REQUIRE_FALSE(m.end_of_session().has_value());
}
