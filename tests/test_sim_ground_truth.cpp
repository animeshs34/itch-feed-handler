#include <catch2/catch_test_macros.hpp>

#include "fh/sim/ground_truth.hpp"

namespace {

fh::sim::GroundMessage filler(fh::Seq seq, char type, fh::InstrumentId locate) {
  fh::sim::GroundMessage m;
  m.seq = seq;
  m.filler = type;
  m.locate = locate;
  return m;
}

fh::sim::GroundMessage book(const fh::BookEvent& e) {
  fh::sim::GroundMessage m;
  m.seq = e.seq;
  m.event = e;
  return m;
}

}  // namespace

TEST_CASE("an empty ground truth has no events, no instruments and last_seq 0", "[sim]") {
  const fh::sim::GroundTruth truth;
  REQUIRE(truth.last_seq() == 0);
  REQUIRE(truth.events().empty());
  REQUIRE(truth.instruments().empty());
}

TEST_CASE("events() keeps book events in order and drops fillers", "[sim]") {
  fh::sim::GroundTruth truth;
  truth.messages = {
      filler(1, 'S', 0),
      book(fh::make_add(2, 7, 100, fh::Side::Buy, 50, 10)),
      filler(3, 'P', 7),
      book(fh::make_delete(4, 7, 100)),
  };
  const auto events = truth.events();
  REQUIRE(events.size() == 2);
  REQUIRE(events[0].seq == 2);
  REQUIRE(events[1].seq == 4);
  REQUIRE(truth.last_seq() == 4);
}

TEST_CASE("instruments() lists only instruments that have a book event", "[sim]") {
  fh::sim::GroundTruth truth;
  truth.messages = {
      filler(1, 'R', 9),  // named by a directory message but never traded
      book(fh::make_add(2, 7, 100, fh::Side::Buy, 50, 10)),
      book(fh::make_add(3, 3, 101, fh::Side::Sell, 60, 5)),
  };
  const auto insts = truth.instruments();
  REQUIRE(insts == std::set<fh::InstrumentId>{3, 7});
}
