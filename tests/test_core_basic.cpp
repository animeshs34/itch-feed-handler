#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "fh/core.hpp"
#include "packets.hpp"

using fh::Line;

namespace {

std::vector<fh::Output> step(fh::Core& core, const fh::Input& input) {
  std::vector<fh::Output> out;
  core.step(input, out);
  return out;
}

int count_updates(const std::vector<fh::Output>& out) {
  int n = 0;
  for (const fh::Output& o : out) n += std::holds_alternative<fh::Update>(o) ? 1 : 0;
  return n;
}

}  // namespace

TEST_CASE("a new core has seen nothing", "[core]") {
  fh::Core core{fh::CoreConfig{}};
  REQUIRE_FALSE(core.expected().has_value());
  REQUIRE_FALSE(core.export_state().has_value());
  REQUIRE(core.feed_health() == fh::FeedHealth::Live);
}

TEST_CASE("the first message seen sets where the core starts", "[core]") {
  fh::Core core{fh::CoreConfig{}};
  (void)step(
      core,
      fhtest::packet(Line::A, 0, {fhtest::message(fh::make_add(5, 1, 100, fh::Side::Buy, 10, 1))}));
  REQUIRE(core.expected() == 6);
  REQUIRE(core.instruments().global_need() == 4);  // 1 to 4 were never seen
}

TEST_CASE("a heartbeat can be the first thing seen", "[core]") {
  fh::Core core{fh::CoreConfig{}};
  (void)step(core, fhtest::heartbeat(Line::B, 0, 40));
  REQUIRE(core.expected() == 40);
  REQUIRE(core.instruments().global_need() == 39);
}

TEST_CASE("a feed that begins at sequence 1 publishes updates straight away", "[core]") {
  fh::Core core{fh::CoreConfig{}};
  const auto out = step(
      core,
      fhtest::packet(Line::A, 0, {fhtest::message(fh::make_add(1, 1, 100, fh::Side::Buy, 10, 5))}));
  REQUIRE(count_updates(out) == 1);
  REQUIRE(core.instruments().global_need() == 0);
}

TEST_CASE("a message with no book effect uses its sequence number silently", "[core]") {
  fh::Core core{fh::CoreConfig{}};
  const auto out =
      step(core, fhtest::packet(Line::A, 0, {fhtest::no_effect(1), fhtest::no_effect(2)}));
  REQUIRE(count_updates(out) == 0);
  REQUIRE(core.expected() == 3);
}

TEST_CASE("the second copy of a message is counted and produces nothing", "[core]") {
  fh::Core core{fh::CoreConfig{}};
  const auto m = fhtest::message(fh::make_add(1, 1, 100, fh::Side::Buy, 10, 5));
  (void)step(core, fhtest::packet(Line::A, 0, {m}));
  const auto out = step(core, fhtest::packet(Line::B, 1, {m}));
  REQUIRE(out.empty());
  REQUIRE(core.counters().duplicates == 1);
}

TEST_CASE("a copy with different bytes raises a line mismatch alarm", "[core]") {
  fh::Core core{fh::CoreConfig{}};
  auto m = fhtest::message(fh::make_add(1, 1, 100, fh::Side::Buy, 10, 5));
  (void)step(core, fhtest::packet(Line::A, 0, {m}));
  m.digest ^= 1;
  const auto out = step(core, fhtest::packet(Line::B, 1, {m}));
  REQUIRE(out.size() == 1);
  REQUIRE(std::get<fh::Alarm>(out[0]).kind == fh::AlarmKind::LineMismatch);
  REQUIRE(core.counters().line_mismatches == 1);
}

TEST_CASE("ticks run the silence clock even before any packet arrives", "[core]") {
  fh::Core core{fh::CoreConfig{}};
  REQUIRE(step(core, fhtest::tick(0)).empty());
  REQUIRE(step(core, fhtest::tick(10 * fh::kSecond)).empty() == false);  // silence is noticed
  REQUIRE(core.feed_health() == fh::FeedHealth::Stale);
  REQUIRE_FALSE(core.expected().has_value());
}

TEST_CASE("a message handed to the core with an empty packet changes nothing", "[core]") {
  fh::Core core{fh::CoreConfig{}};
  const auto out = step(core, fhtest::packet(Line::A, 0, {}));
  REQUIRE(out.empty());
  REQUIRE_FALSE(core.expected().has_value());
}

TEST_CASE("the export is only offered when every book can be trusted", "[core]") {
  fh::Core core{fh::CoreConfig{}};
  (void)step(
      core,
      fhtest::packet(Line::A, 0, {fhtest::message(fh::make_add(1, 1, 100, fh::Side::Buy, 10, 5))}));
  REQUIRE(core.export_state().has_value());
  (void)step(
      core,
      fhtest::packet(Line::A, 1, {fhtest::message(fh::make_add(3, 1, 101, fh::Side::Buy, 10, 5))}));
  REQUIRE_FALSE(core.export_state().has_value());  // seq 2 is missing, 3 is held
}
