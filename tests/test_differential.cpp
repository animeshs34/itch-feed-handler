#include <catch2/catch_test_macros.hpp>
#include <map>

#include "event_gen.hpp"
#include "fh/order_book.hpp"
#include "oracle_book.hpp"

namespace {

std::vector<oracle::Order> real_orders(const fh::InstrumentBook& book, fh::InstrumentId inst) {
  std::vector<oracle::Order> out;
  for (const auto& [id, o] : book.orders()) {
    out.push_back(oracle::Order{id, inst, o.side, o.price, o.shares});
  }
  return out;  // already sorted by id: std::map iterates in key order
}

std::vector<oracle::Level> real_levels(const fh::InstrumentBook& book) {
  std::vector<oracle::Level> out;
  for (const auto& [key, total] : book.levels().all()) {
    out.push_back(oracle::Level{key.side, key.price, total});
  }
  return out;
}

}  // namespace

// The first mini-campaign: thousands of seeded random streams, and after every
// single event the real book and the oracle must agree on the touched instrument.
TEST_CASE("real book and oracle agree after every event of 2000 random streams", "[differential]") {
  constexpr std::uint64_t seeds = 2000;
  for (std::uint64_t seed = 1; seed <= seeds; ++seed) {
    const auto events = fhtest::generate_valid_events(seed, fhtest::GenConfig{3, 300});
    std::map<fh::InstrumentId, fh::InstrumentBook> real;
    oracle::Book answer_key;

    for (const fh::BookEvent& e : events) {
      const fh::ApplyResult r = real[e.inst].apply(e);
      const bool oracle_ok = answer_key.apply(e);
      const bool agree = oracle_ok && r.ok() &&
                         real_orders(real[e.inst], e.inst) == answer_key.orders_of(e.inst) &&
                         real_levels(real[e.inst]) == answer_key.levels_of(e.inst);
      // The check runs 600,000 times, so the failure message is built only when it is needed.
      if (!agree) {
        INFO("seed " << seed << " seq " << e.seq << " oracle_ok " << oracle_ok << " book_ok "
                     << r.ok());
        REQUIRE(agree);
      }
    }
  }
}

TEST_CASE("the generator is deterministic for a seed", "[differential]") {
  const auto a = fhtest::generate_valid_events(7, fhtest::GenConfig{3, 100});
  const auto b = fhtest::generate_valid_events(7, fhtest::GenConfig{3, 100});
  const auto c = fhtest::generate_valid_events(8, fhtest::GenConfig{3, 100});
  REQUIRE(a == b);
  REQUIRE_FALSE(a == c);
  REQUIRE(a.size() == 100);
  REQUIRE(a.front().seq == 1);
  REQUIRE(a.back().seq == 100);
}
