#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "fh/nasdaq/itch_decoder.hpp"
#include "fh/nasdaq/itch_encoder.hpp"
#include "itch_golden.hpp"

using namespace fh::nasdaq;

TEST_CASE("the golden Add decodes to the right event", "[itch_decoder]") {
  const ItchResult r = decode_itch(fhtest::golden_add());
  REQUIRE(r.kind == ItchKind::Event);
  REQUIRE(r.type == 'A');
  REQUIRE(r.event == fh::make_add(0, 1, 101, fh::Side::Buy, 1502500, 100));
}

TEST_CASE("the golden Execute and Cancel both become a Reduce", "[itch_decoder]") {
  const ItchResult e = decode_itch(fhtest::golden_execute());
  REQUIRE(e.kind == ItchKind::Event);
  REQUIRE(e.event == fh::make_reduce(0, 1, 101, 40));

  const ItchResult x = decode_itch(fhtest::golden_cancel());
  REQUIRE(x.kind == ItchKind::Event);
  REQUIRE(x.event == fh::make_reduce(0, 1, 101, 10));
}

TEST_CASE("the golden Replace and Delete decode", "[itch_decoder]") {
  const ItchResult u = decode_itch(fhtest::golden_replace());
  REQUIRE(u.kind == ItchKind::Event);
  REQUIRE(u.event == fh::make_replace(0, 1, 101, 102, 1503000, 70));

  const ItchResult d = decode_itch(fhtest::golden_delete());
  REQUIRE(d.kind == ItchKind::Event);
  REQUIRE(d.event == fh::make_delete(0, 1, 102));
}

TEST_CASE("Add with a participant id decodes like Add", "[itch_decoder]") {
  const ItchHeader h{7, 0, 1000};
  const ItchResult r =
      decode_itch(encode_add_mpid(h, 55, fh::Side::Sell, 9, "ABCD", 12345, "NSDQ"));
  REQUIRE(r.kind == ItchKind::Event);
  REQUIRE(r.type == 'F');
  REQUIRE(r.event == fh::make_add(0, 7, 55, fh::Side::Sell, 12345, 9));
}

TEST_CASE("Execute with price reads the printable flag and the price", "[itch_decoder]") {
  const ItchHeader h{3, 0, 0};
  const ItchResult r = decode_itch(encode_executed_with_price(h, 9, 4, 77, 'Y', 100));
  REQUIRE(r.kind == ItchKind::Event);
  REQUIRE(r.event == fh::make_reduce(0, 3, 9, 4));
}

TEST_CASE("the instrument id is the stock locate", "[itch_decoder]") {
  const ItchResult r = decode_itch(encode_delete(ItchHeader{65535, 9, 9}, 1));
  REQUIRE(r.event.inst == 65535);
  REQUIRE(r.event.seq == 0);  // the packet decoder fills in the sequence number
}

TEST_CASE("known types that change no book are no-effect", "[itch_decoder]") {
  const ItchHeader h{0, 0, 0};
  for (const char type : std::string("SRHYLVWKJhPQBINO")) {
    INFO("type " << type);
    const ItchResult r = decode_itch(encode_filler(type, h));
    REQUIRE(r.kind == ItchKind::NoEffect);
    REQUIRE(r.type == type);
  }
}

TEST_CASE("an unknown type is reported as unknown", "[itch_decoder]") {
  const std::vector<std::uint8_t> bytes = {'Z', 1, 2, 3};
  const ItchResult r = decode_itch(bytes);
  REQUIRE(r.kind == ItchKind::Unknown);
  REQUIRE(r.type == 'Z');
}

TEST_CASE("an empty message is malformed", "[itch_decoder]") {
  const ItchResult r = decode_itch(std::span<const std::uint8_t>{});
  REQUIRE(r.kind == ItchKind::Malformed);
  REQUIRE(r.error == ItchError::Empty);
}

TEST_CASE("a known type with the wrong length is malformed", "[itch_decoder]") {
  auto too_short = fhtest::golden_add();
  too_short.pop_back();
  REQUIRE(decode_itch(too_short).error == ItchError::WrongLength);

  auto too_long = fhtest::golden_delete();
  too_long.push_back(0);
  REQUIRE(decode_itch(too_long).error == ItchError::WrongLength);

  auto cut_filler = encode_filler('S', ItchHeader{});
  cut_filler.pop_back();
  REQUIRE(decode_itch(cut_filler).error == ItchError::WrongLength);
}

TEST_CASE("a bad side byte is rejected", "[itch_decoder]") {
  auto bytes = fhtest::golden_add();
  bytes[19] = 'X';  // the buy/sell indicator
  const ItchResult r = decode_itch(bytes);
  REQUIRE(r.kind == ItchKind::Malformed);
  REQUIRE(r.error == ItchError::BadSide);
}

TEST_CASE("a price above the limit is rejected and the limit itself is allowed", "[itch_decoder]") {
  const ItchHeader h{1, 0, 0};
  auto at_limit = encode_add(h, 1, fh::Side::Buy, 1, "A", 2'000'000'000);
  REQUIRE(decode_itch(at_limit).kind == ItchKind::Event);

  // 0x77359401 is one above the limit.
  at_limit[32] = 0x77;
  at_limit[33] = 0x35;
  at_limit[34] = 0x94;
  at_limit[35] = 0x01;
  const ItchResult r = decode_itch(at_limit);
  REQUIRE(r.kind == ItchKind::Malformed);
  REQUIRE(r.error == ItchError::BadPrice);

  auto replace = encode_replace(h, 1, 2, 5, 100);
  replace[31] = 0xFF;  // makes the price field huge
  REQUIRE(decode_itch(replace).error == ItchError::BadPrice);
}

TEST_CASE("zero shares are rejected wherever the book needs shares", "[itch_decoder]") {
  const ItchHeader h{1, 0, 0};
  auto add = encode_add(h, 1, fh::Side::Buy, 1, "A", 100);
  add[20] = add[21] = add[22] = add[23] = 0;
  REQUIRE(decode_itch(add).error == ItchError::ZeroShares);

  auto execute = encode_executed(h, 1, 1, 1);
  execute[19] = execute[20] = execute[21] = execute[22] = 0;
  REQUIRE(decode_itch(execute).error == ItchError::ZeroShares);

  auto cancel = encode_cancel(h, 1, 1);
  cancel[19] = cancel[20] = cancel[21] = cancel[22] = 0;
  REQUIRE(decode_itch(cancel).error == ItchError::ZeroShares);

  auto replace = encode_replace(h, 1, 2, 1, 100);
  replace[27] = replace[28] = replace[29] = replace[30] = 0;
  REQUIRE(decode_itch(replace).error == ItchError::ZeroShares);
}

TEST_CASE("a bad printable flag is rejected", "[itch_decoder]") {
  const ItchHeader h{1, 0, 0};
  const ItchResult r = decode_itch(encode_executed_with_price(h, 1, 1, 1, 'Q', 100));
  REQUIRE(r.kind == ItchKind::Malformed);
  REQUIRE(r.error == ItchError::BadFlag);
}

TEST_CASE("tracking number and timestamp do not affect the event", "[itch_decoder]") {
  const auto a = decode_itch(encode_delete(ItchHeader{4, 1, 111}, 9));
  const auto b = decode_itch(encode_delete(ItchHeader{4, 999, 222222}, 9));
  REQUIRE(a.event == b.event);
}
