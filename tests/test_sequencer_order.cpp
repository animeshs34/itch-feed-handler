#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "seq_helpers.hpp"

using fhtest::flatten;
using fhtest::msg;

TEST_CASE("messages in order are released at once", "[sequencer]") {
  fh::Sequencer seq(fhtest::sequencer_config(), 1);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(1), 0, out);
  seq.accept(msg(2), 0, out);
  const auto f = flatten(out);
  REQUIRE(f.messages == std::vector<fh::Seq>{1, 2});
  REQUIRE(f.gaps.empty());
  REQUIRE(seq.expected() == 3);
  REQUIRE(seq.buffered() == 0);
}

TEST_CASE("a message ahead of the expected one is held until the hole fills", "[sequencer]") {
  fh::Sequencer seq(fhtest::sequencer_config(), 1);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(3), 10, out);
  seq.accept(msg(2), 11, out);
  REQUIRE(out.empty());
  REQUIRE(seq.buffered() == 2);
  REQUIRE(seq.expected() == 1);

  seq.accept(msg(1), 12, out);
  REQUIRE(flatten(out).messages == std::vector<fh::Seq>{1, 2, 3});
  REQUIRE(seq.buffered() == 0);
  REQUIRE(seq.expected() == 4);
}

TEST_CASE("the sequencer starts wherever it is told to", "[sequencer]") {
  fh::Sequencer seq(fhtest::sequencer_config(), 100);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(100), 0, out);
  REQUIRE(flatten(out).messages == std::vector<fh::Seq>{100});
  REQUIRE(seq.expected() == 101);
}

TEST_CASE("offering the same held message twice keeps one copy", "[sequencer]") {
  fh::Sequencer seq(fhtest::sequencer_config(), 1);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(3), 0, out);
  seq.accept(msg(3), 1, out);
  REQUIRE(seq.buffered() == 1);
}

TEST_CASE("a message below the expected number is counted and dropped", "[sequencer]") {
  fh::Sequencer seq(fhtest::sequencer_config(), 5);
  std::vector<fh::SeqEvent> out;
  seq.accept(msg(2), 0, out);
  REQUIRE(out.empty());
  REQUIRE(seq.late_after_gap() == 1);
  REQUIRE(seq.expected() == 5);
}

TEST_CASE("the configuration is kept", "[sequencer]") {
  const fh::Sequencer seq(fhtest::sequencer_config(7 * fhtest::kUnit, 9), 1);
  REQUIRE(seq.config().skew_window == 7 * fhtest::kUnit);
  REQUIRE(seq.config().buffer_capacity == 9);
}
