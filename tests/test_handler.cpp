#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "byte_truth.hpp"
#include "consumer_model.hpp"
#include "fh/checksum.hpp"
#include "fh/nasdaq/handler.hpp"
#include "timeline.hpp"
#include "walkthrough.hpp"

using fh::Line;
using fhtest::ByteTimeline;

namespace {

constexpr fh::Nanos kUnit = ByteTimeline::kUnit;

fh::CoreConfig config() {
  fh::CoreConfig c;
  c.sequencer.skew_window = 3 * kUnit;
  c.lines.silent_window = 7 * kUnit;
  return c;
}

struct Result {
  fh::nasdaq::Handler handler{config()};
  std::vector<fh::Output> outputs;
  std::vector<fh::GapDeclared> gaps;
  std::size_t updates{0};
};

Result run(const fhtest::ByteTruth& truth, const ByteTimeline& timeline, fh::Nanos end_unit,
           fhtest::ConsumerModel& consumer) {
  Result r;
  for (const fhtest::ByteStep& step : timeline.build(end_unit)) {
    std::vector<fh::Output> out;
    switch (step.kind) {
      case fhtest::ByteStep::Kind::Main:
        r.handler.on_packet(step.t, step.line, step.bytes, out);
        break;
      case fhtest::ByteStep::Kind::Snapshot:
        r.handler.on_snapshot_packet(step.t, step.bytes, out);
        break;
      case fhtest::ByteStep::Kind::Tick:
        r.handler.on_tick(step.t, out);
        break;
    }
    for (const fh::Output& o : out) {
      consumer.consume(o);
      if (const auto* gap = std::get_if<fh::GapDeclared>(&o)) r.gaps.push_back(*gap);
      if (std::holds_alternative<fh::Update>(o)) ++r.updates;
      r.outputs.push_back(o);
    }
  }
  (void)truth;
  return r;
}

void require_ends_equal_to_oracle(const Result& r, const fhtest::ByteTruth& truth) {
  const auto state = r.handler.core().export_state();
  REQUIRE(state.has_value());
  const fh::FeedState expected = fhtest::oracle_state(truth.events, truth.messages.size());
  REQUIRE(*state == expected);
  REQUIRE(fh::checksum(*state) == fh::checksum(expected));
}

void require_consumer_ok(const fhtest::ConsumerModel& consumer) {
  for (const std::string& f : consumer.failures()) UNSCOPED_INFO(f);
  REQUIRE(consumer.ok());
}

}  // namespace

TEST_CASE("clean feed at byte level: every message once", "[handler]") {
  const auto truth = fhtest::make_byte_truth(fhtest::walkthrough_events(), 0);
  ByteTimeline t(truth);
  t.deliver_all(Line::A, fhtest::seqs(1, 10), 0);
  t.deliver_all(Line::B, fhtest::seqs(1, 10), 1);
  fhtest::ConsumerModel consumer(truth.events);
  const Result r = run(truth, t, 12, consumer);

  require_consumer_ok(consumer);
  REQUIRE(r.gaps.empty());
  REQUIRE(r.updates == 10);
  REQUIRE(r.handler.core().counters().duplicates == 10);
  REQUIRE(r.handler.packet_stats().messages == 20);
  require_ends_equal_to_oracle(r, truth);
}

TEST_CASE("both lines lose a burst at byte level: gap then snapshots", "[handler]") {
  const auto truth = fhtest::make_byte_truth(fhtest::walkthrough_events(), 0);
  ByteTimeline t(truth);
  t.deliver_all(Line::A, {1, 2, 3, 6, 7, 8}, 0);
  t.deliver(Line::A, 14, 9);
  t.deliver(Line::A, 15, 10);
  t.deliver_all(Line::B, {1, 2, 3, 6, 7, 8}, 1);
  t.deliver(Line::B, 15, 9);
  t.deliver(Line::B, 16, 10);
  t.snapshot(10, 1, 4);
  t.snapshot(11, 2, 6);
  t.snapshot(12, 1, 10);
  t.snapshot(13, 2, 8);
  fhtest::ConsumerModel consumer(truth.events);
  const Result r = run(truth, t, 17, consumer);

  require_consumer_ok(consumer);
  REQUIRE(r.gaps == std::vector<fh::GapDeclared>{fh::GapDeclared{4, 5, fh::GapReason::Timeout}});
  REQUIRE(r.updates == 7);
  REQUIRE(r.handler.core().counters().stale_snapshots == 1);
  REQUIRE(r.handler.core().counters().ignored_snapshots == 1);
  require_ends_equal_to_oracle(r, truth);
}

TEST_CASE("a damaged packet on one line is covered by the other", "[handler]") {
  const auto truth = fhtest::make_byte_truth(fhtest::walkthrough_events(), 0);
  ByteTimeline t(truth);
  t.deliver_all(Line::A, fhtest::seqs(1, 10, {4}), 0);
  auto damaged = fh::nasdaq::encode_packet(t.main_session(), 4,
                                           std::vector<fh::nasdaq::Bytes>{truth.messages[3]});
  damaged.pop_back();  // cut short
  t.deliver_bytes(Line::A, 4, damaged);
  t.deliver_all(Line::B, fhtest::seqs(1, 10), 1);
  fhtest::ConsumerModel consumer(truth.events);
  const Result r = run(truth, t, 14, consumer);

  require_consumer_ok(consumer);
  REQUIRE(r.gaps.empty());
  REQUIRE(r.handler.packet_stats()
              .packet_errors[static_cast<std::size_t>(fh::nasdaq::PacketError::BlockOverrun)] == 1);
  require_ends_equal_to_oracle(r, truth);
}

TEST_CASE("a message that is bad on both lines becomes a gap and is repaired", "[handler]") {
  // Message 5 (a cancel of order 101) carries a share count of zero on both lines,
  // so the decoder drops it from both and the core sees a hole at 5 that only a
  // snapshot can repair.
  const auto truth = fhtest::make_byte_truth(fhtest::walkthrough_events(), 0);
  auto ruined = truth;
  for (std::size_t i = 19; i < 23; ++i) ruined.messages[4][i] = 0;
  ByteTimeline t(ruined);
  t.deliver_all(Line::A, fhtest::seqs(1, 10), 0);
  t.deliver_all(Line::B, fhtest::seqs(1, 10), 1);
  t.snapshot(12, 1, 10);
  t.snapshot(13, 2, 10);
  fhtest::ConsumerModel consumer(truth.events);
  const Result r = run(ruined, t, 16, consumer);

  require_consumer_ok(consumer);
  REQUIRE(r.gaps == std::vector<fh::GapDeclared>{fh::GapDeclared{5, 5, fh::GapReason::Timeout}});
  REQUIRE(r.handler.packet_stats().malformed_messages == 2);  // one copy on each line
  REQUIRE(r.handler.packet_stats()
              .itch_errors[static_cast<std::size_t>(fh::nasdaq::ItchError::ZeroShares)] == 2);
  require_ends_equal_to_oracle(r, truth);
}

TEST_CASE("messages that change no book use sequence numbers and cause no gap", "[handler]") {
  const auto truth = fhtest::make_byte_truth(fhtest::walkthrough_events(), 2);
  ByteTimeline t(truth);
  const auto all = fhtest::seqs(1, truth.messages.size());
  t.deliver_all(Line::A, all, 0);
  t.deliver_all(Line::B, all, 1);
  fhtest::ConsumerModel consumer(truth.events);
  const Result r = run(truth, t, static_cast<fh::Nanos>(truth.messages.size()) + 4, consumer);

  require_consumer_ok(consumer);
  REQUIRE(truth.messages.size() == 15);  // 10 events and 5 fillers
  REQUIRE(r.gaps.empty());
  REQUIRE(r.updates == 10);
  require_ends_equal_to_oracle(r, truth);
}

TEST_CASE("a lost piece of a snapshot is noticed and the next loop repairs the book", "[handler]") {
  const auto truth = fhtest::make_byte_truth(fhtest::walkthrough_events(), 0);
  ByteTimeline t(truth);
  t.deliver_all(Line::A, {1, 2, 3, 6, 7, 8, 9, 10}, 0);
  t.deliver_all(Line::B, {1, 2, 3, 6, 7, 8, 9, 10}, 1);

  // X's snapshot as of 10 is R, A, A, G. Send it in three packets and lose the middle one.
  const auto run_x = fhtest::snapshot_messages(1, 10, fhtest::oracle_orders(truth.events, 1, 10));
  REQUIRE(run_x.size() == 4);
  t.snapshot_raw(10, 1, {run_x[0], run_x[1]});  // numbers 1 and 2
  // numbers 3 is lost: run_x[2]
  t.snapshot_raw(11, 4, {run_x[3]});  // number 4: the numbering jumped
  t.advance_stream(4);
  t.snapshot(13, 1, 10);  // a whole run in one packet
  t.snapshot(14, 2, 10);
  fhtest::ConsumerModel consumer(truth.events);
  const Result r = run(truth, t, 17, consumer);

  require_consumer_ok(consumer);
  // The broken run was thrown away: X was repaired exactly once, by the whole run.
  REQUIRE(r.handler.core().instruments().states().at(1).epoch() == 1);
  require_ends_equal_to_oracle(r, truth);
}

TEST_CASE("a second session on a line is ignored and the line looks silent", "[handler]") {
  const auto truth = fhtest::make_byte_truth(fhtest::walkthrough_events(), 0);
  ByteTimeline t(truth);
  t.deliver_all(Line::A, fhtest::seqs(1, 10), 0);
  // Line B carries a different session throughout: every packet is refused.
  for (fh::Seq s = 1; s <= 10; ++s) {
    const std::vector<fh::nasdaq::Bytes> one = {truth.messages[s - 1]};
    t.deliver_bytes(Line::B, static_cast<fh::Nanos>(s) + 1,
                    fh::nasdaq::encode_packet(fh::nasdaq::make_session("OTHERSESS"), s, one));
  }
  fhtest::ConsumerModel consumer(truth.events);
  const Result r = run(truth, t, 16, consumer);
  require_consumer_ok(consumer);
  REQUIRE(r.gaps.empty());
  REQUIRE(r.handler.packet_stats()
              .packet_errors[static_cast<std::size_t>(fh::nasdaq::PacketError::SessionMismatch)] >=
          1);
  REQUIRE(r.handler.core().feed_health() == fh::FeedHealth::Degraded);  // line B was never heard
}

TEST_CASE("an end-of-session packet ends the feed health", "[handler]") {
  const auto truth = fhtest::make_byte_truth(fhtest::walkthrough_events(), 0);
  ByteTimeline t(truth);
  t.deliver_all(Line::A, fhtest::seqs(1, 10), 0);
  t.deliver_all(Line::B, fhtest::seqs(1, 10), 1);
  t.end_of_session(Line::A, 12, 11);
  fhtest::ConsumerModel consumer(truth.events);
  const Result r = run(truth, t, 30, consumer);
  require_consumer_ok(consumer);
  REQUIRE(r.handler.core().feed_health() == fh::FeedHealth::Ended);
}
