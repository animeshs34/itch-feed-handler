#include <catch2/catch_test_macros.hpp>
#include <map>

#include "fh/sim/day.hpp"
#include "fh/sim/delivery.hpp"

namespace {

struct Fixture {
  fh::sim::GroundTruth day = fh::sim::make_day(1, fh::sim::DayConfig{});
  fhtest::SplitMix64 rng{1};
  std::vector<fh::Nanos> sent =
      fh::sim::send_times(day.messages.size(), 100 * fh::kMicrosecond, rng, 0);
  std::vector<fh::sim::Datagram> a = fh::sim::packetize(day, sent, fh::Line::A, rng, 3);
  std::vector<fh::sim::Datagram> b = fh::sim::packetize(day, sent, fh::Line::B, rng, 3);
  fh::Nanos end = sent.back();
};

}  // namespace

TEST_CASE("with no faults every datagram arrives once, later by at most jitter plus skew",
          "[sim][delivery]") {
  Fixture f;
  fh::sim::FaultConfig faults;
  faults.jitter = 300 * fh::kMicrosecond;
  faults.skew_b = 150 * fh::kMicrosecond;
  const auto got = fh::sim::deliver_line(f.b, faults, f.end, {}, f.rng);
  REQUIRE(got.size() == f.b.size());
  for (std::size_t i = 0; i < got.size(); ++i) {
    REQUIRE(got[i].entry.t >= f.b[i].sent + faults.skew_b);
    REQUIRE(got[i].entry.t <= f.b[i].sent + faults.skew_b + faults.jitter);
    REQUIRE(got[i].entry.bytes == f.b[i].bytes);
    REQUIRE(got[i].genuine);
    REQUIRE(got[i].entry.line == fh::Line::B);
  }
}

TEST_CASE("loss removes about the stated share, and only in the exposed part", "[sim][delivery]") {
  Fixture f;
  fh::sim::FaultConfig faults;
  faults.loss_pct = 30;
  const fh::Nanos faulty_until = f.end / 2;
  const auto got = fh::sim::deliver_line(f.a, faults, faulty_until, {}, f.rng);

  std::size_t exposed = 0, clean = 0, got_exposed = 0, got_clean = 0;
  for (const auto& d : f.a) (d.sent < faulty_until ? exposed : clean)++;
  for (const auto& g : got) (g.entry.t < faulty_until ? got_exposed : got_clean)++;
  REQUIRE(got_clean == clean);  // nothing lost in the clean part
  REQUIRE(got_exposed < exposed);
  REQUIRE(got_exposed > exposed / 3);  // roughly 70% survive, with wide margin
}

TEST_CASE("a burst removes the same stretch of sending time on both lines", "[sim][delivery]") {
  Fixture f;
  fh::sim::FaultConfig faults;
  const std::vector<fh::sim::Burst> bursts{{2 * fh::kMillisecond, 4 * fh::kMillisecond}};
  for (const auto* line : {&f.a, &f.b}) {
    const auto got = fh::sim::deliver_line(*line, faults, f.end, bursts, f.rng);
    for (const auto& g : got) {
      // find the datagram by its first_seq; its send time must be outside the burst
      const auto it = std::find_if(line->begin(), line->end(),
                                   [&](const auto& d) { return d.first_seq == g.first_seq; });
      const bool inside = it->sent >= bursts[0].from && it->sent < bursts[0].to;
      REQUIRE_FALSE(inside);
    }
    REQUIRE(got.size() < line->size());
  }
}

TEST_CASE("duplicates arrive later than the original and carry the same bytes", "[sim][delivery]") {
  Fixture f;
  fh::sim::FaultConfig faults;
  faults.dup_pct = 50;
  faults.jitter = 200 * fh::kMicrosecond;
  const auto got = fh::sim::deliver_line(f.a, faults, f.end + 1, {}, f.rng);
  REQUIRE(got.size() > f.a.size());
  std::map<fh::Seq, std::vector<fh::Nanos>> times;
  for (const auto& g : got) times[g.first_seq].push_back(g.entry.t);
  std::size_t doubled = 0;
  for (const auto& [seq, ts] : times) {
    REQUIRE(ts.size() <= 2);
    if (ts.size() == 2) {
      ++doubled;
      REQUIRE(ts[1] > ts[0]);
    }
  }
  REQUIRE(doubled > 0);
}

TEST_CASE("bursts are planned only inside the exposed part and never overlap", "[sim][delivery]") {
  fh::sim::FaultConfig faults;
  faults.burst_pct = 10;
  faults.burst_max = 8;
  fhtest::SplitMix64 rng(9);
  const fh::Nanos spacing = 100 * fh::kMicrosecond;
  const fh::Nanos limit = 40 * fh::kMillisecond;
  const auto bursts = fh::sim::make_bursts(faults, limit, spacing, rng);
  REQUIRE_FALSE(bursts.empty());
  fh::Nanos previous_end = 0;
  for (const auto& b : bursts) {
    REQUIRE(b.from < limit);
    REQUIRE(b.from >= previous_end);
    REQUIRE(b.to - b.from >= spacing);
    REQUIRE(b.to - b.from <= 8 * spacing);
    previous_end = b.to;
  }
  REQUIRE(fh::sim::make_bursts(fh::sim::FaultConfig{}, limit, spacing, rng).empty());
}

TEST_CASE("delivery is deterministic for a seed", "[sim][delivery]") {
  Fixture f1;
  Fixture f2;
  fh::sim::FaultConfig faults = fh::sim::tier_config(fh::sim::Tier::Heavy);
  const auto x = fh::sim::deliver_line(f1.a, faults, f1.end, {}, f1.rng);
  const auto y = fh::sim::deliver_line(f2.a, faults, f2.end, {}, f2.rng);
  REQUIRE(x.size() == y.size());
  for (std::size_t i = 0; i < x.size(); ++i) REQUIRE(x[i].entry == y[i].entry);
}

TEST_CASE("a burst placed in the clean part of the day still strikes", "[sim][delivery]") {
  Fixture f;
  const fh::sim::FaultConfig none;
  const std::vector<fh::sim::Burst> end_burst{{f.end - 2 * fh::kMillisecond, f.end + 1}};
  const auto got = fh::sim::deliver_line(f.a, none, 0 /* nothing exposed */, end_burst, f.rng);
  REQUIRE(got.size() < f.a.size());
  for (const auto& g : got) REQUIRE(g.entry.t < f.end - 2 * fh::kMillisecond + 1);
}
