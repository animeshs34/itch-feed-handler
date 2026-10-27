#include <catch2/catch_test_macros.hpp>

#include "fh/sim/scenario.hpp"

namespace {

std::size_t count(const fh::sim::Trace& t, fh::sim::EntryKind kind) {
  std::size_t n = 0;
  for (const auto& e : t.entries) n += e.kind == kind ? 1U : 0U;
  return n;
}

}  // namespace

TEST_CASE("a trace is deterministic for a seed and differs between seeds", "[sim][scenario]") {
  const auto config = fh::sim::scenario_config(fh::sim::Tier::Heavy);
  REQUIRE(fh::sim::make_trace(11, config) == fh::sim::make_trace(11, config));
  REQUIRE_FALSE(fh::sim::make_trace(11, config) == fh::sim::make_trace(12, config));
}

TEST_CASE("entries are in arrival order", "[sim][scenario]") {
  for (unsigned tier = 0; tier < fh::sim::kTierCount; ++tier) {
    const auto trace =
        fh::sim::make_trace(3, fh::sim::scenario_config(static_cast<fh::sim::Tier>(tier)));
    for (std::size_t i = 1; i < trace.entries.size(); ++i) {
      REQUIRE(trace.entries[i].t >= trace.entries[i - 1].t);
    }
  }
}

TEST_CASE("a trace holds both lines, the snapshot stream and ticks", "[sim][scenario]") {
  const auto trace = fh::sim::make_trace(4, fh::sim::scenario_config(fh::sim::Tier::Mild));
  std::size_t a = 0, b = 0;
  for (const auto& e : trace.entries) {
    if (e.kind != fh::sim::EntryKind::Main) continue;
    (e.line == fh::Line::A ? a : b)++;
  }
  REQUIRE(a > 100);
  REQUIRE(b > 100);
  REQUIRE(count(trace, fh::sim::EntryKind::Snapshot) > 10);
  REQUIRE(count(trace, fh::sim::EntryKind::Tick) > 100);
}

TEST_CASE("a clean day delivers every datagram of both lines exactly once", "[sim][scenario]") {
  auto config = fh::sim::scenario_config(fh::sim::Tier::Clean);
  const auto trace = fh::sim::make_trace(5, config);
  // Both lines carry the same messages, so with no loss each message is on the wire twice.
  // Count message datagrams by decoding the count field (bytes 18..19) of every Main entry.
  std::size_t messages_on_wire = 0;
  for (const auto& e : trace.entries) {
    if (e.kind != fh::sim::EntryKind::Main) continue;
    const unsigned count = (static_cast<unsigned>(e.bytes[18]) << 8) | e.bytes[19];
    if (count != 0 && count != 0xFFFF) messages_on_wire += count;
  }
  REQUIRE(messages_on_wire == 2 * trace.truth.messages.size());
}

TEST_CASE("each line ends with an end-of-session packet in a clean day", "[sim][scenario]") {
  const auto trace = fh::sim::make_trace(6, fh::sim::scenario_config(fh::sim::Tier::Clean));
  std::size_t eos_a = 0, eos_b = 0;
  for (const auto& e : trace.entries) {
    if (e.kind != fh::sim::EntryKind::Main) continue;
    if (e.bytes[18] == 0xFF && e.bytes[19] == 0xFF) (e.line == fh::Line::A ? eos_a : eos_b)++;
  }
  REQUIRE(eos_a == 1);
  REQUIRE(eos_b == 1);
}

TEST_CASE("the trace carries the tier's core settings and the true feed", "[sim][scenario]") {
  const auto config = fh::sim::scenario_config(fh::sim::Tier::Brutal);
  const auto trace = fh::sim::make_trace(7, config);
  REQUIRE(trace.core == config.faults.core);
  REQUIRE(trace.seed == 7);
  REQUIRE(trace.truth.messages.size() > 300);
}

TEST_CASE("changing one fault's setting leaves the other faults' draws alone", "[sim][scenario]") {
  // Two traces that differ only in the wrong-session rate must have the same snapshot entries.
  auto base = fh::sim::scenario_config(fh::sim::Tier::Moderate);
  auto other = base;
  other.faults.wrong_session_pct = 0;
  const auto x = fh::sim::make_trace(8, base);
  const auto y = fh::sim::make_trace(8, other);
  std::vector<fh::sim::Entry> sx, sy;
  for (const auto& e : x.entries)
    if (e.kind == fh::sim::EntryKind::Snapshot) sx.push_back(e);
  for (const auto& e : y.entries)
    if (e.kind == fh::sim::EntryKind::Snapshot) sy.push_back(e);
  REQUIRE(sx == sy);
  REQUIRE(x.entries.size() > y.entries.size());
}

TEST_CASE("with end loss at 100 percent the last messages never arrive on either line",
          "[sim][scenario]") {
  auto config = fh::sim::scenario_config(fh::sim::Tier::Clean);
  config.faults.end_loss_pct = 100;
  config.faults.end_loss_max = 5;
  const auto trace = fh::sim::make_trace(3, config);
  // Count the messages that reach the wire: fewer than twice the truth, because the
  // last few are missing on both lines.
  std::size_t messages_on_wire = 0;
  for (const auto& e : trace.entries) {
    if (e.kind != fh::sim::EntryKind::Main) continue;
    const unsigned count = (static_cast<unsigned>(e.bytes[18]) << 8) | e.bytes[19];
    if (count != 0 && count != 0xFFFF) messages_on_wire += count;
  }
  REQUIRE(messages_on_wire < 2 * trace.truth.messages.size());
  REQUIRE(messages_on_wire >= 2 * (trace.truth.messages.size() - 5 - 3));
}

TEST_CASE("make_trace_for with the same day gives the same trace as make_trace",
          "[sim][scenario]") {
  const auto config = fh::sim::scenario_config(fh::sim::Tier::Heavy);
  const auto direct = fh::sim::make_trace(21, config);
  const auto via = fh::sim::make_trace_for(direct.truth, 21, config);
  REQUIRE(via == direct);
}

TEST_CASE("an empty ground truth gives an empty trace", "[sim][scenario]") {
  const auto trace = fh::sim::make_trace_for(fh::sim::GroundTruth{}, 1,
                                             fh::sim::scenario_config(fh::sim::Tier::Mild));
  REQUIRE(trace.entries.empty());
  REQUIRE(trace.truth.messages.empty());
}

TEST_CASE("the same recorded day under two seeds differs only in its faults", "[sim][scenario]") {
  const auto day = fh::sim::make_day(5, fh::sim::DayConfig{});
  const auto config = fh::sim::scenario_config(fh::sim::Tier::Heavy);
  const auto a = fh::sim::make_trace_for(day, 1, config);
  const auto b = fh::sim::make_trace_for(day, 2, config);
  REQUIRE(a.truth == b.truth);
  REQUIRE_FALSE(a.entries == b.entries);
}
