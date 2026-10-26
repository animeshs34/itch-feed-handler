#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "fh/nasdaq/mold_encoder.hpp"
#include "hex.hpp"
#include "itch_golden.hpp"

using namespace fh::nasdaq;

TEST_CASE("a session id is padded with spaces and cut at ten characters", "[mold_encoder]") {
  const SessionId a = make_session("SESSION001");
  REQUIRE(std::string(a.begin(), a.end()) == "SESSION001");
  const SessionId b = make_session("AB");
  REQUIRE(std::string(b.begin(), b.end()) == "AB        ");
  const SessionId c = make_session("0123456789ABC");
  REQUIRE(std::string(c.begin(), c.end()) == "0123456789");
}

TEST_CASE("the encoder reproduces the golden packet byte for byte", "[mold_encoder]") {
  const std::vector<Bytes> messages = {fhtest::golden_add(), fhtest::golden_execute()};
  const Bytes packet = encode_packet(make_session("SESSION001"), 1, messages);
  const Bytes expected = fhtest::from_hex(
      "53 45 53 53 49 4f 4e 30 30 31 00 00 00 00 00 00 00 01 00 02 "
      "00 24 41 00 01 00 00 1f 1a ce d9 f0 00 00 00 00 00 00 00 00 65 42 00 00 00 64 "
      "41 41 50 4c 20 20 20 20 00 16 ed 24 "
      "00 1f 45 00 01 00 00 1f 1a ce d9 f3 e8 00 00 00 00 00 00 00 65 00 00 00 28 "
      "00 00 00 00 00 00 1b 59");
  REQUIRE(packet == expected);
  REQUIRE(packet.size() == 91);
}

TEST_CASE("heartbeat and end of session carry the next number and no body", "[mold_encoder]") {
  const Bytes hb = encode_heartbeat(make_session("SESSION001"), 3);
  REQUIRE(hb == fhtest::from_hex("53 45 53 53 49 4f 4e 30 30 31 00 00 00 00 00 00 00 03 00 00"));
  const Bytes eos = encode_end_of_session(make_session("SESSION001"), 3);
  REQUIRE(eos == fhtest::from_hex("53 45 53 53 49 4f 4e 30 30 31 00 00 00 00 00 00 00 03 ff ff"));
}

TEST_CASE("an empty packet is a heartbeat-shaped header with count zero", "[mold_encoder]") {
  const Bytes packet = encode_packet(make_session("S"), 9, std::span<const Bytes>{});
  REQUIRE(packet.size() == kMoldHeaderBytes);
  REQUIRE(packet[18] == 0);
  REQUIRE(packet[19] == 0);
}
