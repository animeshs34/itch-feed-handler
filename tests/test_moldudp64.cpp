#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "fh/checksum.hpp"
#include "fh/nasdaq/mold_encoder.hpp"
#include "fh/nasdaq/moldudp64.hpp"
#include "hex.hpp"
#include "itch_golden.hpp"

using namespace fh::nasdaq;

namespace {

const SessionId kSession = make_session("SESSION001");

Bytes golden_packet() {
  const std::vector<Bytes> messages = {fhtest::golden_add(), fhtest::golden_execute()};
  return encode_packet(kSession, 1, messages);
}

}  // namespace

TEST_CASE("the golden packet decodes to two sequenced messages", "[mold]") {
  PacketDecoder d;
  const auto p = d.decode(1000, fh::Line::A, golden_packet());
  REQUIRE(p.has_value());
  REQUIRE(p->kind == fh::PacketKind::Messages);
  REQUIRE(p->t == 1000);
  REQUIRE(p->line == fh::Line::A);
  REQUIRE(p->messages.size() == 2);

  REQUIRE(p->messages[0].seq == 1);
  REQUIRE(p->messages[0].event == fh::make_add(1, 1, 101, fh::Side::Buy, 1502500, 100));
  REQUIRE(p->messages[0].digest == fh::fnv1a64(fhtest::golden_add()));
  REQUIRE(p->messages[1].seq == 2);
  REQUIRE(p->messages[1].event == fh::make_reduce(2, 1, 101, 40));
  REQUIRE(d.stats().messages == 2);
  REQUIRE(d.session() == kSession);
}

TEST_CASE("a heartbeat becomes a Heartbeat packet carrying the next number", "[mold]") {
  PacketDecoder d;
  const auto p = d.decode(5, fh::Line::B, encode_heartbeat(kSession, 42));
  REQUIRE(p.has_value());
  REQUIRE(p->kind == fh::PacketKind::Heartbeat);
  REQUIRE(p->next_seq == 42);
  REQUIRE(p->messages.empty());
}

TEST_CASE("an end-of-session packet becomes an EndOfSession packet", "[mold]") {
  PacketDecoder d;
  const auto p = d.decode(5, fh::Line::A, encode_end_of_session(kSession, 42));
  REQUIRE(p.has_value());
  REQUIRE(p->kind == fh::PacketKind::EndOfSession);
  REQUIRE(p->next_seq == 42);
}

TEST_CASE("a packet with fewer than 20 bytes is dropped", "[mold]") {
  PacketDecoder d;
  const Bytes packet = golden_packet();
  for (std::size_t len = 0; len < kMoldHeaderBytes; ++len) {
    const Bytes cut(packet.begin(), packet.begin() + static_cast<std::ptrdiff_t>(len));
    REQUIRE_FALSE(d.decode(0, fh::Line::A, cut).has_value());
  }
  REQUIRE(d.stats().packet_errors[static_cast<std::size_t>(PacketError::TooShort)] == 20);
  REQUIRE_FALSE(d.session().has_value());  // nothing sound has been seen yet
}

TEST_CASE("every truncation of a valid packet is dropped", "[mold]") {
  const Bytes packet = golden_packet();
  for (std::size_t len = 0; len < packet.size(); ++len) {
    PacketDecoder d;
    const Bytes cut(packet.begin(), packet.begin() + static_cast<std::ptrdiff_t>(len));
    INFO("length " << len);
    REQUIRE_FALSE(d.decode(0, fh::Line::A, cut).has_value());
  }
}

TEST_CASE("bytes left over after the last block drop the packet", "[mold]") {
  PacketDecoder d;
  Bytes packet = golden_packet();
  packet.push_back(0);
  REQUIRE_FALSE(d.decode(0, fh::Line::A, packet).has_value());
  REQUIRE(d.stats().packet_errors[static_cast<std::size_t>(PacketError::TrailingBytes)] == 1);
}

TEST_CASE("a block whose length runs past the packet drops it", "[mold]") {
  PacketDecoder d;
  Bytes packet = golden_packet();
  packet[20] = 0xFF;  // the first block now claims to be 0xFF24 bytes long
  REQUIRE_FALSE(d.decode(0, fh::Line::A, packet).has_value());
  REQUIRE(d.stats().packet_errors[static_cast<std::size_t>(PacketError::BlockOverrun)] == 1);
}

TEST_CASE("a count larger than the blocks present drops the packet", "[mold]") {
  PacketDecoder d;
  Bytes packet = golden_packet();
  packet[19] = 5;  // says five messages, holds two
  REQUIRE_FALSE(d.decode(0, fh::Line::A, packet).has_value());
}

TEST_CASE("a heartbeat with extra bytes is dropped", "[mold]") {
  PacketDecoder d;
  Bytes hb = encode_heartbeat(kSession, 3);
  hb.push_back(1);
  REQUIRE_FALSE(d.decode(0, fh::Line::A, hb).has_value());
  REQUIRE(d.stats().packet_errors[static_cast<std::size_t>(PacketError::HeartbeatHasBody)] == 1);
}

TEST_CASE("a packet from another session is dropped once a session is established", "[mold]") {
  PacketDecoder d;
  REQUIRE(d.decode(0, fh::Line::A, golden_packet()).has_value());
  const std::vector<Bytes> one = {fhtest::golden_delete()};
  const Bytes other = encode_packet(make_session("OTHER"), 3, one);
  REQUIRE_FALSE(d.decode(1, fh::Line::A, other).has_value());
  REQUIRE(d.stats().packet_errors[static_cast<std::size_t>(PacketError::SessionMismatch)] == 1);
  REQUIRE(d.decode(2, fh::Line::B, encode_heartbeat(kSession, 3)).has_value());  // ours still works
}

TEST_CASE("sequence numbers that would overflow are rejected", "[mold]") {
  PacketDecoder d;
  const std::vector<Bytes> two = {fhtest::golden_delete(), fhtest::golden_delete()};
  const Bytes packet = encode_packet(kSession, 0xFFFFFFFFFFFFFFFFULL, two);
  REQUIRE_FALSE(d.decode(0, fh::Line::A, packet).has_value());
  REQUIRE(d.stats().packet_errors[static_cast<std::size_t>(PacketError::BadSequence)] == 1);
}

TEST_CASE("messages that change no book keep their sequence numbers", "[mold]") {
  PacketDecoder d;
  const std::vector<Bytes> messages = {encode_system_event(ItchHeader{}, 'O'), fhtest::golden_add(),
                                       encode_stock_directory(ItchHeader{1, 0, 0}, "AAPL")};
  const auto p = d.decode(0, fh::Line::A, encode_packet(kSession, 10, messages));
  REQUIRE(p.has_value());
  REQUIRE(p->messages.size() == 3);
  REQUIRE_FALSE(p->messages[0].event.has_value());
  REQUIRE(p->messages[0].seq == 10);
  REQUIRE(p->messages[1].event.has_value());
  REQUIRE(p->messages[1].seq == 11);
  REQUIRE(p->messages[1].event->seq == 11);
  REQUIRE_FALSE(p->messages[2].event.has_value());
  REQUIRE(p->messages[2].seq == 12);
}

TEST_CASE("an unknown message type is passed on as a no-effect message and counted", "[mold]") {
  PacketDecoder d;
  const std::vector<Bytes> messages = {Bytes{'Z', 1, 2, 3}, fhtest::golden_add()};
  const auto p = d.decode(0, fh::Line::A, encode_packet(kSession, 1, messages));
  REQUIRE(p.has_value());
  REQUIRE(p->messages.size() == 2);  // it used up sequence number 1; dropping it would fake a loss
  REQUIRE_FALSE(p->messages[0].event.has_value());
  REQUIRE(p->messages[1].seq == 2);
  REQUIRE(d.stats().unknown_types == 1);
}

TEST_CASE("one malformed message is dropped and the rest of the packet is kept", "[mold]") {
  PacketDecoder d;
  Bytes bad = fhtest::golden_add();
  bad[19] = 'X';  // bad side
  const std::vector<Bytes> messages = {fhtest::golden_add(), bad, fhtest::golden_delete()};
  const auto p = d.decode(0, fh::Line::A, encode_packet(kSession, 1, messages));
  REQUIRE(p.has_value());
  REQUIRE(p->messages.size() == 2);
  REQUIRE(p->messages[0].seq == 1);
  REQUIRE(p->messages[1].seq == 3);  // a hole at 2: the core will see it as a loss
  REQUIRE(d.stats().malformed_messages == 1);
  REQUIRE(d.stats().itch_errors[static_cast<std::size_t>(ItchError::BadSide)] == 1);
}

TEST_CASE("an empty message block is a malformed message, not a crash", "[mold]") {
  PacketDecoder d;
  const std::vector<Bytes> messages = {Bytes{}, fhtest::golden_delete()};
  const auto p = d.decode(0, fh::Line::A, encode_packet(kSession, 1, messages));
  REQUIRE(p.has_value());
  REQUIRE(p->messages.size() == 1);
  REQUIRE(d.stats().itch_errors[static_cast<std::size_t>(ItchError::Empty)] == 1);
}

TEST_CASE("every packet is accounted for", "[mold]") {
  PacketDecoder d;
  std::uint64_t accepted = 0;
  const Bytes good = golden_packet();
  Bytes broken = good;
  broken.pop_back();
  for (int i = 0; i < 5; ++i) accepted += d.decode(0, fh::Line::A, good).has_value() ? 1U : 0U;
  for (int i = 0; i < 3; ++i) accepted += d.decode(0, fh::Line::A, broken).has_value() ? 1U : 0U;
  std::uint64_t errors = 0;
  for (const auto n : d.stats().packet_errors) errors += n;
  REQUIRE(d.stats().packets == 8);
  REQUIRE(accepted + errors == d.stats().packets);
}

TEST_CASE("a zero-length packet of 65534 empty blocks is bounded by its size", "[mold]") {
  // A header claiming 65534 blocks but carrying none must fail at once, not loop for long.
  Bytes packet = encode_heartbeat(kSession, 1);
  packet[18] = 0xFF;
  packet[19] = 0xFE;
  PacketDecoder d;
  REQUIRE_FALSE(d.decode(0, fh::Line::A, packet).has_value());
}
