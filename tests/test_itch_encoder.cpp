#include <catch2/catch_test_macros.hpp>
#include <string>

#include "fh/nasdaq/itch_encoder.hpp"
#include "fh/nasdaq/itch_layout.hpp"
#include "itch_golden.hpp"

using namespace fh::nasdaq;

namespace {
constexpr std::uint64_t kNineThirty = 34'200'000'000'000ULL;  // 09:30:00 in ns
}

TEST_CASE("the encoder reproduces the golden vectors byte for byte", "[itch_encoder]") {
  REQUIRE(encode_add(ItchHeader{1, 0, kNineThirty}, 101, fh::Side::Buy, 100, "AAPL", 1502500) ==
          fhtest::golden_add());
  REQUIRE(encode_executed(ItchHeader{1, 0, kNineThirty + 1000}, 101, 40, 7001) ==
          fhtest::golden_execute());
  REQUIRE(encode_cancel(ItchHeader{1, 0, kNineThirty + 2000}, 101, 10) == fhtest::golden_cancel());
  REQUIRE(encode_replace(ItchHeader{1, 0, kNineThirty + 3000}, 101, 102, 70, 1503000) ==
          fhtest::golden_replace());
  REQUIRE(encode_delete(ItchHeader{1, 0, kNineThirty + 4000}, 102) == fhtest::golden_delete());
}

TEST_CASE("every message has the length the specification gives", "[itch_encoder]") {
  const ItchHeader h{5, 0, 1};
  REQUIRE(encode_add(h, 1, fh::Side::Buy, 1, "A", 1).size() == 36);
  REQUIRE(encode_add_mpid(h, 1, fh::Side::Buy, 1, "A", 1, "NSDQ").size() == 40);
  REQUIRE(encode_executed(h, 1, 1, 1).size() == 31);
  REQUIRE(encode_executed_with_price(h, 1, 1, 1, 'Y', 1).size() == 36);
  REQUIRE(encode_cancel(h, 1, 1).size() == 23);
  REQUIRE(encode_delete(h, 1).size() == 19);
  REQUIRE(encode_replace(h, 1, 2, 1, 1).size() == 35);
  REQUIRE(encode_system_event(h, 'O').size() == 12);
  REQUIRE(encode_stock_directory(h, "AAPL").size() == 39);
  REQUIRE(encode_trade(h, 1, "AAPL", 1, 1).size() == 44);
}

TEST_CASE("fillers exist for every type that changes no book", "[itch_encoder]") {
  for (const MessageSpec& spec : kMessageSpecs) {
    if (spec.affects_book) continue;
    INFO("type " << spec.type);
    const Bytes m = encode_filler(spec.type, ItchHeader{9, 0, 5});
    REQUIRE(m.size() == spec.length);
    REQUIRE(m[0] == static_cast<std::uint8_t>(spec.type));
  }
}

TEST_CASE("the trade message has a zero order reference and a buy side", "[itch_encoder]") {
  const Bytes m = encode_trade(ItchHeader{2, 0, 0}, 5, "MSFT", 100, 9);
  for (std::size_t i = 11; i < 19; ++i) REQUIRE(m[i] == 0);  // order reference
  REQUIRE(m[19] == 'B');
}

TEST_CASE("alpha fields are padded on the right with spaces", "[itch_encoder]") {
  const Bytes m = encode_add(ItchHeader{1, 0, 0}, 1, fh::Side::Sell, 1, "AB", 1);
  REQUIRE(std::string(m.begin() + 24, m.begin() + 32) == "AB      ");
}

TEST_CASE("the end-of-snapshot message is G and a right-justified 20 character number",
          "[itch_encoder]") {
  const Bytes g = encode_end_of_snapshot(8);
  REQUIRE(g.size() == 21);
  REQUIRE(g[0] == 'G');
  REQUIRE(std::string(g.begin() + 1, g.end()) == "                   8");
  const Bytes big = encode_end_of_snapshot(18446744073709551615ULL);
  REQUIRE(std::string(big.begin() + 1, big.end()) == "18446744073709551615");
}

TEST_CASE("synthetic stock names are 8 characters and distinct per instrument", "[itch_encoder]") {
  REQUIRE(synthetic_stock_name(7) == "I7      ");
  REQUIRE(synthetic_stock_name(65535) == "I65535  ");
  REQUIRE(synthetic_stock_name(1) != synthetic_stock_name(2));
}

TEST_CASE("a Reduce can be written as an execution or as a cancel", "[itch_encoder]") {
  const fh::BookEvent reduce = fh::make_reduce(5, 3, 77, 10);
  EventStyle execute_style;
  execute_style.reduce_as_execute = true;
  execute_style.match_number = 99;
  EventStyle cancel_style;
  REQUIRE(encode_event(reduce, execute_style)[0] == 'E');
  REQUIRE(encode_event(reduce, cancel_style)[0] == 'X');
}
