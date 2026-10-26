#include <catch2/catch_test_macros.hpp>

#include "fh/sim/day.hpp"
#include "oracle_book.hpp"

TEST_CASE("the day is deterministic for a seed and differs between seeds", "[sim][day]") {
  const fh::sim::DayConfig config;
  REQUIRE(fh::sim::make_day(5, config) == fh::sim::make_day(5, config));
  REQUIRE_FALSE(fh::sim::make_day(5, config) == fh::sim::make_day(6, config));
}

TEST_CASE("the day follows the lifecycle of a real session", "[sim][day]") {
  fh::sim::DayConfig config;
  config.instruments = 3;
  config.book_events = 50;
  config.filler_every = 0;
  const fh::sim::GroundTruth day = fh::sim::make_day(1, config);

  const auto& m = day.messages;
  // S/O, then R H Y for each of the 3 instruments, then S/S and S/Q.
  REQUIRE(m[0].filler == 'S');
  REQUIRE(m[0].code == 'O');
  for (std::size_t i = 0; i < 3; ++i) {
    REQUIRE(m[1 + i].filler == 'R');
    REQUIRE(m[4 + i].filler == 'H');
    REQUIRE(m[7 + i].filler == 'Y');
    REQUIRE(m[1 + i].locate == i + 1);
  }
  REQUIRE(m[10].code == 'S');
  REQUIRE(m[11].code == 'Q');
  REQUIRE(m[12].event.has_value());
  // 12 opening messages + 50 book events + S/M, S/E, S/C.
  REQUIRE(m.size() == 12 + 50 + 3);
  REQUIRE(m[m.size() - 3].code == 'M');
  REQUIRE(m[m.size() - 2].code == 'E');
  REQUIRE(m[m.size() - 1].code == 'C');
}

TEST_CASE("sequence numbers are 1..n with no gaps and event seq matches message seq",
          "[sim][day]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(2, fh::sim::DayConfig{});
  for (std::size_t i = 0; i < day.messages.size(); ++i) {
    REQUIRE(day.messages[i].seq == i + 1);
    if (day.messages[i].event) REQUIRE(day.messages[i].event->seq == i + 1);
  }
}

TEST_CASE("fillers are interleaved with the book events", "[sim][day]") {
  fh::sim::DayConfig config;
  config.book_events = 60;
  config.filler_every = 6;
  const fh::sim::GroundTruth day = fh::sim::make_day(3, config);
  std::size_t trade_like = 0;
  for (const auto& m : day.messages) {
    if (!m.event && m.filler != 'S' && m.filler != 'R' && m.filler != 'H' && m.filler != 'Y') {
      ++trade_like;
    }
  }
  REQUIRE(trade_like == 10);  // 60 events, one filler after every 6th
  REQUIRE(day.events().size() == 60);
}

TEST_CASE("the book events are valid: the oracle accepts every one of them", "[sim][day]") {
  for (std::uint64_t seed = 1; seed <= 200; ++seed) {
    const fh::sim::GroundTruth day = fh::sim::make_day(seed, fh::sim::DayConfig{});
    oracle::Book book;
    for (const fh::BookEvent& e : day.events()) {
      INFO("seed " << seed << " seq " << e.seq);
      REQUIRE(book.apply(e));
    }
  }
}
