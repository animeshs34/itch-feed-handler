#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

#include "fh/state_codec.hpp"
#include "state_fixtures.hpp"

using Bytes = std::vector<std::uint8_t>;

TEST_CASE("encode produces the exact documented bytes (golden vector)", "[encode]") {
  const Bytes expected = {
      0x46, 0x48, 0x42, 0x4B,                          // magic "FHBK"
      0x01, 0x00,                                      // version 1
      0xFC,                                            // price_exp -4
      0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // seq 7
      0x01, 0x00, 0x00, 0x00,                          // 1 instrument
      0x01, 0x00,                                      // instrument id 1
      0x01, 0x00, 0x00, 0x00,                          // 1 order
      0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // order id 101
      0x42,                                            // side 'B'
      0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // price 100
      0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // shares 10
  };
  REQUIRE(fh::encode(fhtest::one_order_state()) == expected);
}

TEST_CASE("an empty state encodes to just the 19 byte header", "[encode]") {
  REQUIRE(fh::encode(fh::FeedState{}).size() == 19);
}

TEST_CASE("negative prices and the exponent are written as two's complement", "[encode]") {
  fh::FeedState s;
  s.price_exp = -2;
  REQUIRE(s.books[1].apply(fh::make_add(1, 1, 1, fh::Side::Sell, -1, 1)).ok());
  const Bytes bytes = fh::encode(s);
  REQUIRE(bytes[6] == 0xFE);  // price_exp -2
  // price field: after header 19 + instrument 6 + order id 8 + side 1 = offset 34
  for (std::size_t i = 34; i < 42; ++i) REQUIRE(bytes[i] == 0xFF);  // -1 as int64
  REQUIRE(bytes[33] == 'S');
}

TEST_CASE("instruments are written in id order whatever order they were created in", "[encode]") {
  fh::FeedState a, b;
  a.books[9];
  a.books[2];
  b.books[2];
  b.books[9];
  REQUIRE(fh::encode(a) == fh::encode(b));
  const Bytes bytes = fh::encode(a);
  REQUIRE(bytes[19] == 2);  // first instrument id (low byte) is the smaller one
}

TEST_CASE("orders are written in id order whatever order they arrived in", "[encode]") {
  fh::FeedState a, b;
  REQUIRE(a.books[1].apply(fh::make_add(1, 1, 5, fh::Side::Buy, 100, 1)).ok());
  REQUIRE(a.books[1].apply(fh::make_add(2, 1, 3, fh::Side::Buy, 100, 1)).ok());
  REQUIRE(b.books[1].apply(fh::make_add(1, 1, 3, fh::Side::Buy, 100, 1)).ok());
  REQUIRE(b.books[1].apply(fh::make_add(2, 1, 5, fh::Side::Buy, 100, 1)).ok());
  a.seq = b.seq = 2;
  REQUIRE(fh::encode(a) == fh::encode(b));
}
