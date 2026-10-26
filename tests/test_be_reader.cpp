#include <catch2/catch_test_macros.hpp>

#include "fh/nasdaq/be_reader.hpp"
#include "hex.hpp"

using fh::nasdaq::BeReader;

TEST_CASE("integers are read most significant byte first", "[be_reader]") {
  const auto bytes = fhtest::from_hex("12 34 56 78 9a bc de f0");
  BeReader r(bytes);
  REQUIRE(r.u8() == 0x12);
  REQUIRE(r.u16() == 0x3456);
  REQUIRE(r.remaining() == 5);
  REQUIRE(r.take(5) == 0x789abcdef0ULL);
  REQUIRE(r.remaining() == 0);
}

TEST_CASE("a 32 bit and a 64 bit read", "[be_reader]") {
  const auto bytes = fhtest::from_hex("00 00 01 00 00 00 00 00 00 00 ff ff");
  BeReader r(bytes);
  REQUIRE(r.u32() == 256);
  REQUIRE(r.u64() == 0xffff);
}

TEST_CASE("a 6 byte timestamp is read as one number", "[be_reader]") {
  const auto bytes = fhtest::from_hex("1f 1a ce d9 f0 00");
  BeReader r(bytes);
  REQUIRE(r.take(6) == 34'200'000'000'000ULL);  // 09:30:00 in nanoseconds
}

TEST_CASE("a read past the end fails and consumes nothing", "[be_reader]") {
  const auto bytes = fhtest::from_hex("01 02 03");
  BeReader r(bytes);
  REQUIRE_FALSE(r.u32().has_value());
  REQUIRE(r.position() == 0);
  REQUIRE(r.remaining() == 3);
  REQUIRE(r.u16() == 0x0102);
  REQUIRE_FALSE(r.u16().has_value());
  REQUIRE(r.remaining() == 1);
}

TEST_CASE("reading from an empty span always fails", "[be_reader]") {
  BeReader r(std::span<const std::uint8_t>{});
  REQUIRE_FALSE(r.u8().has_value());
  REQUIRE_FALSE(r.take_bytes(1).has_value());
  REQUIRE_FALSE(r.skip(1));
  REQUIRE(r.take_bytes(0).has_value());  // taking nothing is fine
  REQUIRE(r.skip(0));
}

TEST_CASE("take_bytes returns a view of the original and advances", "[be_reader]") {
  const auto bytes = fhtest::from_hex("aa bb cc dd");
  BeReader r(bytes);
  const auto view = r.take_bytes(3);
  REQUIRE(view.has_value());
  REQUIRE(view->size() == 3);
  REQUIRE(view->data() == bytes.data());
  REQUIRE(r.u8() == 0xdd);
}

TEST_CASE("skip moves forward only when enough bytes remain", "[be_reader]") {
  const auto bytes = fhtest::from_hex("01 02 03 04");
  BeReader r(bytes);
  REQUIRE(r.skip(3));
  REQUIRE_FALSE(r.skip(2));
  REQUIRE(r.remaining() == 1);
}
