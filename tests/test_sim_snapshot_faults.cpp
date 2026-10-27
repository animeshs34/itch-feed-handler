#include <catch2/catch_test_macros.hpp>
#include <map>

#include "fh/nasdaq/snapshot_decoder.hpp"
#include "fh/sim/day.hpp"
#include "fh/sim/snapshot_faults.hpp"

namespace {

struct Fixture {
  fh::sim::GroundTruth day = fh::sim::make_day(1, fh::sim::DayConfig{});
  fhtest::SplitMix64 rng{1};
  fh::sim::Timing timing;
  std::vector<fh::Nanos> sent = fh::sim::send_times(day.messages.size(), timing.spacing, rng, 0);
  fh::Nanos end = sent.back() + 20 * fh::kMillisecond;
};

// The runs that arrived complete, as run keys: (instrument, asof) from the decoded close.
std::size_t count_closes(const std::vector<fh::sim::Arrival>& arrivals) {
  fh::nasdaq::SnapshotStreamDecoder decoder;
  std::size_t closes = 0;
  for (const auto& a : arrivals) {
    const auto p = decoder.decode(a.entry.t, a.entry.bytes);
    REQUIRE(p.has_value());
    for (const auto& item : p->items)
      closes += std::holds_alternative<fh::SnapshotClose>(item) ? 1U : 0U;
  }
  return closes;
}

}  // namespace

TEST_CASE("with no faults one run per period arrives, instruments in turn", "[sim][snapshot]") {
  Fixture f;
  const auto arrivals =
      fh::sim::deliver_snapshots(f.day, f.sent, f.timing, fh::sim::FaultConfig{}, 0, f.end, f.rng);
  const auto expected_runs = static_cast<std::size_t>(f.end / f.timing.snapshot_period);
  REQUIRE(count_closes(arrivals) == expected_runs);

  fh::nasdaq::SnapshotStreamDecoder decoder;
  std::vector<fh::InstrumentId> order;
  for (const auto& a : arrivals) {
    const auto p = decoder.decode(a.entry.t, a.entry.bytes);
    for (const auto& item : p->items) {
      if (const auto* open = std::get_if<fh::SnapshotOpen>(&item)) order.push_back(open->inst);
    }
  }
  for (std::size_t i = 0; i < order.size(); ++i) REQUIRE(order[i] == 1 + (i % 4));
}

TEST_CASE("the as-of of a run is the number of messages sent by then", "[sim][snapshot]") {
  Fixture f;
  const auto arrivals =
      fh::sim::deliver_snapshots(f.day, f.sent, f.timing, fh::sim::FaultConfig{}, 0, f.end, f.rng);
  fh::nasdaq::SnapshotStreamDecoder decoder;
  std::size_t run = 0;
  for (const auto& a : arrivals) {
    const auto p = decoder.decode(a.entry.t, a.entry.bytes);
    for (const auto& item : p->items) {
      if (const auto* close = std::get_if<fh::SnapshotClose>(&item)) {
        ++run;
        const fh::Nanos t = static_cast<fh::Nanos>(run) * f.timing.snapshot_period;
        const auto sent_by = static_cast<fh::Seq>(
            std::upper_bound(f.sent.begin(), f.sent.end(), t) - f.sent.begin());
        REQUIRE(close->asof == sent_by);
      }
    }
  }
}

TEST_CASE("whole-run loss removes runs, but only in the exposed part", "[sim][snapshot]") {
  Fixture f;
  fh::sim::FaultConfig faults;
  faults.snap_loss_pct = 50;
  const fh::Nanos limit = f.end / 2;
  const auto arrivals =
      fh::sim::deliver_snapshots(f.day, f.sent, f.timing, faults, limit, f.end, f.rng);
  const auto all = static_cast<std::size_t>(f.end / f.timing.snapshot_period);
  const auto clean_runs = static_cast<std::size_t>((f.end - limit) / f.timing.snapshot_period);
  const std::size_t got = count_closes(arrivals);
  REQUIRE(got < all);
  REQUIRE(got >= clean_runs);
}

TEST_CASE("split-run loss leaves a run without its close or its start", "[sim][snapshot]") {
  Fixture f;
  fh::sim::FaultConfig faults;
  faults.snap_split_loss_pct = 100;
  const auto arrivals =
      fh::sim::deliver_snapshots(f.day, f.sent, f.timing, faults, f.end, f.end, f.rng);
  fh::sim::FaultConfig none;
  fhtest::SplitMix64 rng2(1);
  const auto whole = fh::sim::deliver_snapshots(f.day, f.sent, f.timing, none, 0, f.end, rng2);
  REQUIRE(arrivals.size() < whole.size());
  // Every run of two or more datagrams lost one, so the stream numbering has a hole:
  // some datagram does not start where the previous one ended.
  std::map<fh::Seq, fh::Seq> end_of;  // first -> one past the last message
  for (const auto& a : arrivals) end_of[a.first_seq] = a.first_seq + a.count;
  bool hole = false;
  fh::Seq previous_end = 1;
  for (const auto& [first, end_seq] : end_of) {
    if (first != previous_end) hole = true;
    previous_end = end_seq;
  }
  REQUIRE(hole);
}

TEST_CASE("late runs arrive at least 6 ms after they were sent", "[sim][snapshot]") {
  Fixture f;
  fh::sim::FaultConfig faults;
  faults.snap_late_pct = 100;
  const auto arrivals =
      fh::sim::deliver_snapshots(f.day, f.sent, f.timing, faults, f.end, f.end, f.rng);
  fhtest::SplitMix64 rng2(1);
  const auto prompt =
      fh::sim::deliver_snapshots(f.day, f.sent, f.timing, fh::sim::FaultConfig{}, 0, f.end, rng2);
  REQUIRE(arrivals.size() == prompt.size());
  fh::Nanos late_sum = 0, prompt_sum = 0;
  for (const auto& a : arrivals) late_sum += a.entry.t;
  for (const auto& a : prompt) prompt_sum += a.entry.t;
  REQUIRE((late_sum - prompt_sum) / static_cast<fh::Nanos>(arrivals.size()) > 5 * fh::kMillisecond);
}

TEST_CASE("duplicated datagrams carry the same bytes and arrive later", "[sim][snapshot]") {
  Fixture f;
  fh::sim::FaultConfig faults;
  faults.snap_dup_pct = 100;
  const auto arrivals =
      fh::sim::deliver_snapshots(f.day, f.sent, f.timing, faults, f.end, f.end, f.rng);
  std::map<fh::Seq, std::vector<const fh::sim::Arrival*>> by_first;
  for (const auto& a : arrivals) by_first[a.first_seq].push_back(&a);
  for (const auto& [first, copies] : by_first) {
    REQUIRE(copies.size() == 2);
    REQUIRE(copies[0]->entry.bytes == copies[1]->entry.bytes);
    REQUIRE(copies[1]->entry.t > copies[0]->entry.t);
  }
}
