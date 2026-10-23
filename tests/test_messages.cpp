#include <catch2/catch_test_macros.hpp>

#include "fh/messages.hpp"
#include "fh/outputs.hpp"
#include "packets.hpp"

TEST_CASE("lines index the arrays that hold per-line data", "[messages]") {
  REQUIRE(fh::index_of(fh::Line::A) == 0);
  REQUIRE(fh::index_of(fh::Line::B) == 1);
  REQUIRE(fh::kLineCount == 2);
}

TEST_CASE("time units are consistent", "[messages]") {
  REQUIRE(fh::kMillisecond == 1000 * fh::kMicrosecond);
  REQUIRE(fh::kSecond == 1000 * fh::kMillisecond);
}

TEST_CASE("sequenced messages compare field by field", "[messages]") {
  const fh::BookEvent e = fh::make_delete(5, 1, 100);
  REQUIRE(fhtest::message(e) == fhtest::message(e));
  REQUIRE_FALSE(fhtest::message(e) == fhtest::no_effect(5));
  fh::SequencedMessage altered = fhtest::message(e);
  altered.digest ^= 1;
  REQUIRE_FALSE(altered == fhtest::message(e));  // same event, different bytes
}

TEST_CASE("a message with no book effect still carries its sequence number", "[messages]") {
  const fh::SequencedMessage m = fhtest::no_effect(42, 7);
  REQUIRE(m.seq == 42);
  REQUIRE_FALSE(m.event.has_value());
}

TEST_CASE("packet builders set the kind and the time", "[messages]") {
  const fh::PacketIn hb = fhtest::heartbeat(fh::Line::B, 1000, 9);
  REQUIRE(hb.kind == fh::PacketKind::Heartbeat);
  REQUIRE(hb.next_seq == 9);
  REQUIRE(hb.t == 1000);
  REQUIRE(hb.line == fh::Line::B);
  REQUIRE(fhtest::end_of_session(fh::Line::A, 5, 3).kind == fh::PacketKind::EndOfSession);
  REQUIRE(fhtest::packet(fh::Line::A, 1, {fhtest::no_effect(1)}).kind == fh::PacketKind::Messages);
}

TEST_CASE("snapshot run items are Open then orders then Close", "[messages]") {
  const auto items = fhtest::run_items(
      7, 12, {{1, fh::Order{fh::Side::Buy, 100, 5}}, {2, fh::Order{fh::Side::Sell, 101, 6}}});
  REQUIRE(items.size() == 4);
  REQUIRE(std::holds_alternative<fh::SnapshotOpen>(items[0]));
  REQUIRE(std::holds_alternative<fh::SnapshotOrder>(items[1]));
  REQUIRE(std::holds_alternative<fh::SnapshotOrder>(items[2]));
  REQUIRE(std::get<fh::SnapshotClose>(items[3]).asof == 12);
}

TEST_CASE("outputs compare equal when their contents are equal", "[messages]") {
  REQUIRE(fh::Output{fh::GapDeclared{1, 2, fh::GapReason::Timeout}} ==
          fh::Output{fh::GapDeclared{1, 2, fh::GapReason::Timeout}});
  REQUIRE_FALSE(fh::Output{fh::GapDeclared{1, 2, fh::GapReason::Timeout}} ==
                fh::Output{fh::GapDeclared{1, 3, fh::GapReason::Timeout}});
  REQUIRE_FALSE(fh::Output{fh::FeedHealthChange{fh::FeedHealth::Live}} ==
                fh::Output{fh::GapDeclared{1, 2, fh::GapReason::Timeout}});
}
