#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "consumer_model.hpp"
#include "fh/checksum.hpp"
#include "fh/core.hpp"
#include "timeline.hpp"
#include "walkthrough.hpp"

using fh::Line;
using fhtest::Timeline;

namespace {

constexpr fh::Nanos kUnit = Timeline::kUnit;

fh::CoreConfig config() {
  fh::CoreConfig c;
  c.sequencer.skew_window = 3 * kUnit;
  c.lines.silent_window = 7 * kUnit;
  return c;
}

struct Result {
  fh::Core core{config()};
  std::vector<fh::Output> outputs;
  std::vector<fh::GapDeclared> gaps;
  std::vector<fh::FeedHealth> feed_changes;
  std::size_t updates{0};
};

// Runs a timeline through a fresh core and checks every output as a consumer would.
Result run(const std::vector<fh::BookEvent>& truth, const Timeline& timeline, fh::Nanos end_unit,
           fhtest::ConsumerModel& consumer) {
  Result r;
  for (const fh::Input& input : timeline.build(end_unit)) {
    std::vector<fh::Output> out;
    r.core.step(input, out);
    for (const fh::Output& o : out) {
      consumer.consume(o);
      if (const auto* gap = std::get_if<fh::GapDeclared>(&o)) r.gaps.push_back(*gap);
      if (const auto* feed = std::get_if<fh::FeedHealthChange>(&o))
        r.feed_changes.push_back(feed->state);
      if (std::holds_alternative<fh::Update>(o)) ++r.updates;
      r.outputs.push_back(o);
    }
  }
  (void)truth;
  return r;
}

void require_ends_equal_to_oracle(const Result& r, const std::vector<fh::BookEvent>& truth) {
  const auto state = r.core.export_state();
  REQUIRE(state.has_value());
  const fh::FeedState expected = fhtest::oracle_state(truth, truth.size());
  REQUIRE(*state == expected);
  REQUIRE(fh::checksum(*state) == fh::checksum(expected));
}

void require_consumer_ok(const fhtest::ConsumerModel& consumer) {
  for (const std::string& f : consumer.failures()) UNSCOPED_INFO(f);
  REQUIRE(consumer.ok());
}

}  // namespace

TEST_CASE("clean feed: every message once and the copies recognised", "[scenario]") {
  const auto truth = fhtest::walkthrough_events();
  Timeline t(truth);
  t.deliver_all(Line::A, fhtest::seqs(1, 10), 0);
  t.deliver_all(Line::B, fhtest::seqs(1, 10), 1);
  fhtest::ConsumerModel consumer(truth);
  const Result r = run(truth, t, 12, consumer);

  require_consumer_ok(consumer);
  REQUIRE(r.gaps.empty());
  REQUIRE(r.updates == 10);
  REQUIRE(r.core.counters().duplicates == 10);
  require_ends_equal_to_oracle(r, truth);
}

TEST_CASE("one line loses a burst and the other covers it with no gap", "[scenario]") {
  const auto truth = fhtest::walkthrough_events();
  Timeline t(truth);
  t.deliver_all(Line::A, fhtest::seqs(1, 10, {3, 4}), 0);
  t.deliver_all(Line::B, fhtest::seqs(1, 10), 1);
  fhtest::ConsumerModel consumer(truth);
  const Result r = run(truth, t, 12, consumer);

  require_consumer_ok(consumer);
  REQUIRE(r.gaps.empty());
  REQUIRE(r.updates == 10);
  REQUIRE(r.core.counters().duplicates == 8);
  require_ends_equal_to_oracle(r, truth);
}

TEST_CASE("a slow line fills the hole inside the window so no gap is declared", "[scenario]") {
  const auto truth = fhtest::walkthrough_events();
  Timeline t(truth);
  t.deliver_all(Line::A, fhtest::seqs(1, 10, {5}), 0);
  t.deliver_all(Line::B, fhtest::seqs(1, 10), 3);
  fhtest::ConsumerModel consumer(truth);
  const Result r = run(truth, t, 14, consumer);

  require_consumer_ok(consumer);
  REQUIRE(r.gaps.empty());
  REQUIRE(r.core.counters().duplicates == 9);
  require_ends_equal_to_oracle(r, truth);
}

TEST_CASE("both lines lose a burst: gap then snapshots (stale, usable, ahead)", "[scenario]") {
  const auto truth = fhtest::walkthrough_events();
  Timeline t(truth);
  t.deliver_all(Line::A, {1, 2, 3, 6, 7, 8}, 0);
  t.deliver(Line::A, 14, 9);
  t.deliver(Line::A, 15, 10);
  t.deliver_all(Line::B, {1, 2, 3, 6, 7, 8}, 1);
  t.deliver(Line::B, 15, 9);
  t.deliver(Line::B, 16, 10);
  t.snapshot(10, 1, 4);   // X as of 4: older than the gap end (5), stale
  t.snapshot(11, 2, 6);   // Y as of 6: usable, and seq 7 was parked after it
  t.snapshot(12, 1, 10);  // X as of 10: ahead of the incremental feed (it is at 8)
  t.snapshot(13, 2, 8);   // Y again, but Y is already live
  fhtest::ConsumerModel consumer(truth);
  const Result r = run(truth, t, 17, consumer);

  require_consumer_ok(consumer);
  REQUIRE(r.gaps == std::vector<fh::GapDeclared>{fh::GapDeclared{4, 5, fh::GapReason::Timeout}});
  REQUIRE(r.updates == 7);  // seqs 1 2 3, Reset Y, delta 7, Reset X, delta 9
  const auto& states = r.core.instruments().states();
  REQUIRE(states.at(1).epoch() == 1);
  REQUIRE(states.at(1).last_seq() == 10);
  REQUIRE(states.at(2).epoch() == 1);
  REQUIRE(states.at(2).last_seq() == 9);
  REQUIRE(r.core.counters().stale_snapshots == 1);
  REQUIRE(r.core.counters().ignored_snapshots == 1);
  REQUIRE(r.core.counters().covered == 1);  // seq 10 arrives after the X snapshot already covers it
  require_ends_equal_to_oracle(r, truth);
}

TEST_CASE("tail loss is found by a heartbeat and repaired by snapshots", "[scenario]") {
  const auto truth = fhtest::walkthrough_events();
  Timeline t(truth);
  t.deliver_all(Line::A, fhtest::seqs(1, 8), 0);
  t.deliver_all(Line::B, fhtest::seqs(1, 8), 1);
  t.heartbeat(Line::A, 9, 9);  // publisher will send 9 next: nothing missing
  t.heartbeat(Line::B, 10, 9);
  t.heartbeat(Line::A, 11, 11);  // 9 and 10 were sent but never arrived
  t.heartbeat(Line::B, 12, 11);
  t.snapshot(15, 1, 10);
  t.snapshot(16, 2, 10);
  fhtest::ConsumerModel consumer(truth);
  const Result r = run(truth, t, 17, consumer);

  require_consumer_ok(consumer);
  REQUIRE(r.gaps == std::vector<fh::GapDeclared>{fh::GapDeclared{9, 10, fh::GapReason::Tail}});
  require_ends_equal_to_oracle(r, truth);
}

TEST_CASE("duplicates and reordering are absorbed and altered bytes raise an alarm", "[scenario]") {
  const auto truth = fhtest::walkthrough_events();
  Timeline t(truth);
  t.deliver(Line::A, 1, 1);
  t.deliver(Line::A, 2, 3);  // 3 arrives before 2
  t.deliver(Line::A, 3, 2);
  t.deliver(Line::A, 3, 3);  // the same packet twice
  t.deliver(Line::A, 5, 5);  // 5 arrives before 4
  t.deliver(Line::A, 6, 4);
  for (fh::Seq s = 6; s <= 10; ++s) t.deliver(Line::A, static_cast<fh::Nanos>(s) + 1, s);
  t.deliver_all(Line::B, fhtest::seqs(1, 6), 2);
  t.deliver_altered(Line::B, 9, 7);  // line B's copy of seq 7 has different bytes
  t.deliver_all(Line::B, fhtest::seqs(8, 10), 2);
  fhtest::ConsumerModel consumer(truth);
  const Result r = run(truth, t, 13, consumer);

  require_consumer_ok(consumer);
  REQUIRE(r.gaps.empty());
  REQUIRE(r.core.counters().line_mismatches == 1);
  bool alarm_seen = false;
  for (const fh::Output& o : r.outputs) {
    if (const auto* a = std::get_if<fh::Alarm>(&o)) {
      alarm_seen = alarm_seen || (a->kind == fh::AlarmKind::LineMismatch && a->seq == 7);
    }
  }
  REQUIRE(alarm_seen);
  require_ends_equal_to_oracle(r, truth);
}

TEST_CASE("a feed whose first packet is out of order starts from that packet", "[scenario]") {
  // The core cannot tell a reordered start from joining mid-stream, so it treats
  // the first sequence number it sees as the start and repairs the rest from snapshots.
  const auto truth = fhtest::walkthrough_events();
  Timeline t(truth);
  t.deliver(Line::A, 1, 2);  // seq 2 first
  t.deliver(Line::A, 2, 1);  // seq 1 arrives after we started at 2: too late
  t.snapshot(4, 1, 1);
  t.snapshot(5, 2, 2);
  fhtest::ConsumerModel consumer(truth);
  const Result r = run(truth, t, 7, consumer);

  require_consumer_ok(consumer);
  REQUIRE(r.core.instruments().global_need() == 1);
  REQUIRE(r.core.counters().late_after_gap == 1);
  REQUIRE(r.core.instruments().all_live());
}

TEST_CASE("a line goes silent then both: the feed degrades then goes stale", "[scenario]") {
  const auto truth = fhtest::walkthrough_events();
  Timeline t(truth);
  t.deliver_all(Line::A, fhtest::seqs(1, 6), 0);
  t.deliver_all(Line::B, fhtest::seqs(1, 3), 1);
  t.heartbeat(Line::A, 8, 7);
  t.heartbeat(Line::A, 11, 7);
  fhtest::ConsumerModel consumer(truth);
  const Result r = run(truth, t, 19, consumer);

  require_consumer_ok(consumer);
  REQUIRE(r.feed_changes ==
          std::vector<fh::FeedHealth>{fh::FeedHealth::Degraded, fh::FeedHealth::Stale});
  REQUIRE(r.gaps.empty());
  REQUIRE(r.updates == 6);
  REQUIRE(r.core.feed_health() == fh::FeedHealth::Stale);
}

TEST_CASE("the end of the session ends the feed health", "[scenario]") {
  const auto truth = fhtest::walkthrough_events();
  Timeline t(truth);
  t.deliver_all(Line::A, fhtest::seqs(1, 10), 0);
  t.deliver_all(Line::B, fhtest::seqs(1, 10), 1);
  t.end_of_session(Line::A, 12, 11);
  t.end_of_session(Line::B, 12, 11);
  fhtest::ConsumerModel consumer(truth);
  const Result r = run(truth, t, 30, consumer);

  require_consumer_ok(consumer);
  // Ended, and never Degraded or Stale: silence after the end is expected.
  REQUIRE(r.feed_changes == std::vector<fh::FeedHealth>{fh::FeedHealth::Ended});
  REQUIRE(r.gaps.empty());
}

TEST_CASE("joining mid-stream: everything before is recovered from snapshots", "[scenario]") {
  const auto truth = fhtest::walkthrough_events();
  Timeline t(truth);
  for (fh::Seq s = 6; s <= 10; ++s) t.deliver(Line::A, static_cast<fh::Nanos>(s) - 5, s);
  t.snapshot(6, 1, 5);  // X as of 5: exactly what is needed (we joined at 6)
  t.snapshot(7, 2, 6);  // Y as of 6
  fhtest::ConsumerModel consumer(truth);
  const Result r = run(truth, t, 9, consumer);

  require_consumer_ok(consumer);
  REQUIRE(r.gaps.empty());
  REQUIRE(r.core.instruments().global_need() == 5);
  require_ends_equal_to_oracle(r, truth);
}

TEST_CASE("a feed that starts at sequence 1 needs no snapshot", "[scenario]") {
  const auto truth = fhtest::walkthrough_events();
  Timeline t(truth);
  t.deliver_all(Line::A, fhtest::seqs(1, 10), 0);
  fhtest::ConsumerModel consumer(truth);
  const Result r = run(truth, t, 12, consumer);
  require_consumer_ok(consumer);
  REQUIRE(r.core.instruments().global_need() == 0);
  REQUIRE(r.core.instruments().all_live());
}
