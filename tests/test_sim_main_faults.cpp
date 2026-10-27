#include <catch2/catch_test_macros.hpp>

#include "fh/nasdaq/moldudp64.hpp"
#include "fh/sim/day.hpp"
#include "fh/sim/main_faults.hpp"

namespace {

struct Fixture {
  fh::sim::GroundTruth day = fh::sim::make_day(1, fh::sim::DayConfig{});
  fhtest::SplitMix64 rng{1};
  std::vector<fh::Nanos> sent =
      fh::sim::send_times(day.messages.size(), 100 * fh::kMicrosecond, rng, 0);
  fh::Nanos end = sent.back();

  // Both lines delivered with no loss, the base for every fault below.
  std::vector<fh::sim::Arrival> base() {
    fh::sim::FaultConfig none;
    none.jitter = 100 * fh::kMicrosecond;
    auto a = fh::sim::deliver_line(fh::sim::packetize(day, sent, fh::Line::A, rng, 3), none, end,
                                   {}, rng);
    auto b = fh::sim::deliver_line(fh::sim::packetize(day, sent, fh::Line::B, rng, 3), none, end,
                                   {}, rng);
    a.insert(a.end(), b.begin(), b.end());
    return a;
  }
};

}  // namespace

TEST_CASE("silence removes one line, or both, for longer than the silent window", "[sim][faults]") {
  Fixture f;
  auto arrivals = f.base();
  const std::size_t before = arrivals.size();
  fh::sim::FaultConfig faults;
  faults.silence_windows = 1;
  fh::sim::apply_silence(arrivals, faults, f.end, 100 * fh::kMicrosecond, f.rng);
  REQUIRE(arrivals.size() < before);

  // Whichever line (or both) was silenced has a hole of at least 8 ms between neighbours.
  fh::Nanos hole = 0;
  for (const fh::Line line : {fh::Line::A, fh::Line::B}) {
    std::vector<fh::Nanos> times;
    for (const auto& a : arrivals) {
      if (a.entry.line == line) times.push_back(a.entry.t);
    }
    std::sort(times.begin(), times.end());
    for (std::size_t i = 1; i < times.size(); ++i) hole = std::max(hole, times[i] - times[i - 1]);
  }
  REQUIRE(hole >= 8 * fh::kMillisecond);
}

TEST_CASE("no silence windows means nothing is removed", "[sim][faults]") {
  Fixture f;
  auto arrivals = f.base();
  const std::size_t before = arrivals.size();
  fh::sim::apply_silence(arrivals, fh::sim::FaultConfig{}, f.end, 100 * fh::kMicrosecond, f.rng);
  REQUIRE(arrivals.size() == before);
}

TEST_CASE("every malformed datagram is refused whole by the decoder", "[sim][faults]") {
  Fixture f;
  auto arrivals = f.base();
  fh::sim::FaultConfig faults;
  faults.malformed_pct = 40;
  fh::sim::apply_malformed(arrivals, faults, f.end, f.rng);

  std::size_t cut = 0;
  for (const auto& a : arrivals) {
    if (a.genuine) continue;
    ++cut;
    fh::nasdaq::PacketDecoder decoder;
    // Name the session first with a sound datagram so the refusal is about structure.
    const auto sound = fh::sim::make_heartbeat(0, fh::Line::A, 1);
    REQUIRE(decoder.decode(0, fh::Line::A, sound.bytes).has_value());
    REQUIRE_FALSE(decoder.decode(a.entry.t, a.entry.line, a.entry.bytes).has_value());
  }
  REQUIRE(cut > 20);
}

TEST_CASE("malformed damage stays out of the clean part of the day", "[sim][faults]") {
  Fixture f;
  auto arrivals = f.base();
  fh::sim::FaultConfig faults;
  faults.malformed_pct = 100;
  const fh::Nanos limit = f.end / 2;
  fh::sim::apply_malformed(arrivals, faults, limit, f.rng);
  for (const auto& a : arrivals) {
    if (a.entry.t >= limit) REQUIRE(a.genuine);
  }
}

TEST_CASE("a mismatch copy arrives after a genuine copy of the same messages", "[sim][faults]") {
  Fixture f;
  auto arrivals = f.base();
  const std::size_t before = arrivals.size();
  fh::sim::FaultConfig faults;
  faults.mismatch_pct = 30;
  fh::sim::add_mismatches(arrivals, faults, f.end, f.rng);
  REQUIRE(arrivals.size() > before);

  for (std::size_t i = before; i < arrivals.size(); ++i) {
    const auto& copy = arrivals[i];
    REQUIRE_FALSE(copy.genuine);
    bool earlier_genuine = false;
    for (std::size_t j = 0; j < before; ++j) {
      const auto& g = arrivals[j];
      if (g.entry.line == copy.entry.line && g.first_seq == copy.first_seq &&
          g.entry.t < copy.entry.t && g.entry.bytes.size() == copy.entry.bytes.size() &&
          g.entry.bytes != copy.entry.bytes) {
        earlier_genuine = true;
      }
    }
    REQUIRE(earlier_genuine);
  }
}

TEST_CASE("a mismatch copy differs from the original in exactly one byte", "[sim][faults]") {
  Fixture f;
  auto arrivals = f.base();
  const std::size_t before = arrivals.size();
  fh::sim::FaultConfig faults;
  faults.mismatch_pct = 100;
  fh::sim::add_mismatches(arrivals, faults, f.end, f.rng);
  for (std::size_t i = before; i < arrivals.size(); ++i) {
    std::size_t best = before;  // the original: same line and first_seq, earlier
    for (std::size_t j = 0; j < before; ++j) {
      if (arrivals[j].entry.line == arrivals[i].entry.line &&
          arrivals[j].first_seq == arrivals[i].first_seq &&
          arrivals[j].entry.t < arrivals[i].entry.t) {
        best = j;
      }
    }
    REQUIRE(best < before);
    std::size_t diff = 0;
    for (std::size_t k = 0; k < arrivals[i].entry.bytes.size(); ++k) {
      diff += arrivals[i].entry.bytes[k] != arrivals[best].entry.bytes[k] ? 1U : 0U;
    }
    REQUIRE(diff == 1);
  }
}

TEST_CASE("a wrong-session copy is refused once the session is named", "[sim][faults]") {
  Fixture f;
  auto arrivals = f.base();
  const std::size_t before = arrivals.size();
  fh::sim::FaultConfig faults;
  faults.wrong_session_pct = 20;
  fh::sim::add_wrong_session(arrivals, faults, f.end, f.rng);
  REQUIRE(arrivals.size() > before);
  for (std::size_t i = before; i < arrivals.size(); ++i) {
    REQUIRE_FALSE(arrivals[i].genuine);
    fh::nasdaq::PacketDecoder decoder;
    const auto sound = fh::sim::make_heartbeat(0, fh::Line::A, 1);
    REQUIRE(decoder.decode(0, fh::Line::A, sound.bytes).has_value());
    REQUIRE_FALSE(
        decoder.decode(arrivals[i].entry.t, arrivals[i].entry.line, arrivals[i].entry.bytes)
            .has_value());
  }
}
