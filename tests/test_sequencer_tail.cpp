#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "seq_helpers.hpp"

using fh::GapDeclared;
using fh::GapReason;
using fhtest::flatten;
using fhtest::kUnit;
using fhtest::msg;

TEST_CASE("a heartbeat that shows missing messages starts the wait and ends in a tail gap",
          "[sequencer][tail]") {
  fh::Sequencer seq(fhtest::sequencer_config(3 * kUnit), 1);
  std::vector<fh::SeqEvent> out;
  for (fh::Seq s = 1; s <= 8; ++s) seq.accept(msg(s), 0, out);
  out.clear();

  seq.note_next_expected(11, 10 * kUnit);  // the publisher will send 11 next: 9 and 10 exist
  REQUIRE(seq.has_hole());
  REQUIRE(seq.waiting_since() == 10 * kUnit);

  seq.tick(12 * kUnit, out);
  REQUIRE(out.empty());
  seq.tick(13 * kUnit, out);
  REQUIRE(flatten(out).gaps == std::vector<GapDeclared>{GapDeclared{9, 10, GapReason::Tail}});
  REQUIRE(seq.expected() == 11);
  REQUIRE_FALSE(seq.has_hole());
}

TEST_CASE("a heartbeat that matches what we have changes nothing", "[sequencer][tail]") {
  fh::Sequencer seq(fhtest::sequencer_config(), 1);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(1), 0, out);
  seq.note_next_expected(2, kUnit);  // next is 2, we expect 2: nothing missing
  REQUIRE_FALSE(seq.has_hole());
  REQUIRE_FALSE(seq.waiting_since().has_value());
  seq.note_next_expected(0, kUnit);  // zero is not a valid next number: ignored
  REQUIRE_FALSE(seq.has_hole());
}

TEST_CASE("messages that arrive after a heartbeat clear the tail wait", "[sequencer][tail]") {
  fh::Sequencer seq(fhtest::sequencer_config(), 1);
  std::vector<fh::SeqEvent> out;
  seq.note_next_expected(3, 0);  // 1 and 2 exist
  seq.accept(msg(1), kUnit, out);
  seq.accept(msg(2), kUnit, out);
  REQUIRE_FALSE(seq.has_hole());
  seq.tick(100 * kUnit, out);
  REQUIRE(flatten(out).gaps.empty());
}

TEST_CASE("a gap through a held message leaves a later tail still being waited for",
          "[sequencer][tail]") {
  fh::Sequencer seq(fhtest::sequencer_config(3 * kUnit), 1);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(3), 0, out);    // 1 and 2 missing
  seq.note_next_expected(9, 0);  // and the publisher has sent up to 8
  seq.tick(3 * kUnit, out);      // gap {1,2} through the held message; 3 released
  REQUIRE(flatten(out).gaps == std::vector<GapDeclared>{GapDeclared{1, 2, GapReason::Timeout}});
  REQUIRE(seq.expected() == 4);
  REQUIRE(seq.has_hole());  // 4..8 are still missing
  out.clear();
  seq.tick(6 * kUnit, out);  // a new wait began at the gap
  REQUIRE(flatten(out).gaps == std::vector<GapDeclared>{GapDeclared{4, 8, GapReason::Tail}});
}
