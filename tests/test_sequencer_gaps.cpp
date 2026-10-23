#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "seq_helpers.hpp"

using fh::GapDeclared;
using fh::GapReason;
using fhtest::flatten;
using fhtest::kUnit;
using fhtest::msg;

TEST_CASE("an in-order message leaves nothing waiting", "[sequencer][gaps]") {
  fh::Sequencer seq(fhtest::sequencer_config(), 1);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(1), 0, out);
  REQUIRE_FALSE(seq.has_hole());
  REQUIRE_FALSE(seq.waiting_since().has_value());
}

TEST_CASE("holding a message starts the wait and filling the hole ends it", "[sequencer][gaps]") {
  fh::Sequencer seq(fhtest::sequencer_config(), 1);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(2), 10, out);
  REQUIRE(seq.has_hole());
  REQUIRE(seq.waiting_since() == 10);
  seq.accept(msg(1), 20, out);
  REQUIRE_FALSE(seq.has_hole());
  REQUIRE_FALSE(seq.waiting_since().has_value());
}

TEST_CASE("no gap is declared when the missing message arrives inside the window",
          "[sequencer][gaps]") {
  fh::Sequencer seq(fhtest::sequencer_config(3 * kUnit), 1);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(2), 0, out);
  seq.tick(2 * kUnit, out);  // 2 units waited, the window is 3
  seq.accept(msg(1), 2 * kUnit + 1, out);
  REQUIRE(flatten(out).gaps.empty());
  REQUIRE(flatten(out).messages == std::vector<fh::Seq>{1, 2});
}

TEST_CASE("a gap is declared once the window passes then later messages are released",
          "[sequencer][gaps]") {
  fh::Sequencer seq(fhtest::sequencer_config(3 * kUnit), 1);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(1), 0, out);
  seq.accept(msg(3), 1 * kUnit, out);  // 2 is missing
  seq.accept(msg(4), 1 * kUnit, out);
  out.clear();

  seq.tick(4 * kUnit - 1, out);  // waited 3 units minus 1 ns
  REQUIRE(out.empty());

  seq.tick(4 * kUnit, out);  // waited exactly the window
  const auto f = flatten(out);
  REQUIRE(f.gaps == std::vector<GapDeclared>{GapDeclared{2, 2, GapReason::Timeout}});
  REQUIRE(f.messages == std::vector<fh::Seq>{3, 4});
  REQUIRE(seq.expected() == 5);
  REQUIRE_FALSE(seq.has_hole());
}

TEST_CASE("the gap covers every number up to the first message held", "[sequencer][gaps]") {
  fh::Sequencer seq(fhtest::sequencer_config(), 4);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(7), 0, out);
  seq.tick(3 * kUnit, out);
  REQUIRE(flatten(out).gaps == std::vector<GapDeclared>{GapDeclared{4, 6, GapReason::Timeout}});
  REQUIRE(flatten(out).messages == std::vector<fh::Seq>{7});
}

TEST_CASE("the gap comes before the messages that follow it in the output", "[sequencer][gaps]") {
  fh::Sequencer seq(fhtest::sequencer_config(), 1);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(2), 0, out);
  seq.tick(3 * kUnit, out);
  REQUIRE(out.size() == 2);
  REQUIRE(std::holds_alternative<GapDeclared>(out[0]));
  REQUIRE(std::holds_alternative<fh::SequencedMessage>(out[1]));
}

TEST_CASE("the wait restarts when progress is made but a hole remains", "[sequencer][gaps]") {
  fh::Sequencer seq(fhtest::sequencer_config(2 * kUnit), 1);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(3), 0, out);          // waiting for 1 and 2 since t=0
  seq.accept(msg(1), kUnit / 2, out);  // 1 arrives: still missing 2, a new wait begins
  REQUIRE(seq.waiting_since() == kUnit / 2);
  out.clear();

  seq.tick(2 * kUnit, out);  // 2 units after t=0 but only 1.5 after the restart
  REQUIRE(out.empty());
  seq.tick(kUnit / 2 + 2 * kUnit, out);
  REQUIRE(flatten(out).gaps == std::vector<GapDeclared>{GapDeclared{2, 2, GapReason::Timeout}});
}

TEST_CASE("a full reorder buffer forces a gap at once", "[sequencer][gaps]") {
  fh::Sequencer seq(fhtest::sequencer_config(3 * kUnit, /*capacity=*/2), 1);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(3), 0, out);
  seq.accept(msg(4), 0, out);
  REQUIRE(out.empty());
  seq.accept(msg(5), 0, out);  // a third would exceed the capacity of 2
  const auto f = flatten(out);
  REQUIRE(f.gaps == std::vector<GapDeclared>{GapDeclared{1, 2, GapReason::Overflow}});
  REQUIRE(f.messages == std::vector<fh::Seq>{3, 4, 5});
  REQUIRE(std::holds_alternative<GapDeclared>(out.front()));
  REQUIRE(seq.buffered() == 0);
}

TEST_CASE("a message that arrives after its number was given up on is counted and dropped",
          "[sequencer][gaps]") {
  fh::Sequencer seq(fhtest::sequencer_config(), 1);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(2), 0, out);
  seq.tick(3 * kUnit, out);  // gap {1,1}
  out.clear();
  seq.accept(msg(1), 4 * kUnit, out);
  REQUIRE(out.empty());
  REQUIRE(seq.late_after_gap() == 1);
}
