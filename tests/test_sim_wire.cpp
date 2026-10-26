#include <catch2/catch_test_macros.hpp>
#include <set>

#include "fh/nasdaq/moldudp64.hpp"
#include "fh/sim/day.hpp"
#include "fh/sim/wire.hpp"

namespace {

// Decodes a message datagram back into messages, using the real decoder.
std::vector<fh::SequencedMessage> decode(const fh::sim::Datagram& d) {
  fh::nasdaq::PacketDecoder decoder;
  auto packet = decoder.decode(d.sent, d.line, d.bytes);
  REQUIRE(packet.has_value());
  return packet->messages;
}

}  // namespace

TEST_CASE("every true message survives encode and decode", "[sim][wire]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(4, fh::sim::DayConfig{});
  fhtest::SplitMix64 rng(1);
  const auto sent = fh::sim::send_times(day.messages.size(), 100 * fh::kMicrosecond, rng, 0);
  const auto datagrams = fh::sim::packetize(day, sent, fh::Line::A, rng, 3);

  std::size_t next = 0;
  for (const auto& d : datagrams) {
    for (const fh::SequencedMessage& m : decode(d)) {
      const fh::sim::GroundMessage& truth = day.messages[next++];
      REQUIRE(m.seq == truth.seq);
      REQUIRE(m.event == truth.event);
    }
  }
  REQUIRE(next == day.messages.size());
}

TEST_CASE("packetizing covers every sequence number exactly once, in order", "[sim][wire]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(5, fh::sim::DayConfig{});
  fhtest::SplitMix64 rng(2);
  const auto sent = fh::sim::send_times(day.messages.size(), 100 * fh::kMicrosecond, rng, 0);
  const auto datagrams = fh::sim::packetize(day, sent, fh::Line::B, rng, 4);

  fh::Seq expected = 1;
  for (const auto& d : datagrams) {
    REQUIRE(d.first_seq == expected);
    REQUIRE(d.count >= 1);
    REQUIRE(d.count <= 4);
    REQUIRE(d.line == fh::Line::B);
    expected += d.count;
  }
  REQUIRE(expected == day.messages.size() + 1);
}

TEST_CASE("a datagram leaves when its last message is sent", "[sim][wire]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(6, fh::sim::DayConfig{});
  fhtest::SplitMix64 rng(3);
  const auto sent = fh::sim::send_times(day.messages.size(), 100 * fh::kMicrosecond, rng, 0);
  for (const auto& d : fh::sim::packetize(day, sent, fh::Line::A, rng, 5)) {
    REQUIRE(d.sent == sent[d.first_seq + d.count - 2]);
  }
}

TEST_CASE("the two lines cut the feed at different places", "[sim][wire]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(7, fh::sim::DayConfig{});
  fhtest::SplitMix64 rng(4);
  const auto sent = fh::sim::send_times(day.messages.size(), 100 * fh::kMicrosecond, rng, 0);
  const auto a = fh::sim::packetize(day, sent, fh::Line::A, rng, 4);
  const auto b = fh::sim::packetize(day, sent, fh::Line::B, rng, 4);
  std::set<fh::Seq> cuts_a, cuts_b;
  for (const auto& d : a) cuts_a.insert(d.first_seq);
  for (const auto& d : b) cuts_b.insert(d.first_seq);
  REQUIRE(cuts_a != cuts_b);
}

TEST_CASE("send times never decrease, and bursts squeeze messages together", "[sim][wire]") {
  fhtest::SplitMix64 rng(5);
  const auto calm = fh::sim::send_times(500, 100 * fh::kMicrosecond, rng, 0);
  REQUIRE(calm.front() == 100 * fh::kMicrosecond);
  REQUIRE(calm.back() == 500 * 100 * fh::kMicrosecond);

  const auto bursty = fh::sim::send_times(500, 100 * fh::kMicrosecond, rng, 100);
  std::size_t squeezed = 0;
  for (std::size_t i = 1; i < bursty.size(); ++i) {
    REQUIRE(bursty[i] >= bursty[i - 1]);
    if (bursty[i] - bursty[i - 1] == fh::kMicrosecond) ++squeezed;
  }
  REQUIRE(squeezed > 10);
  REQUIRE(bursty.back() < calm.back());
}

TEST_CASE("heartbeat and end-of-session datagrams carry the next sequence number", "[sim][wire]") {
  const auto hb = fh::sim::make_heartbeat(5 * fh::kMillisecond, fh::Line::B, 42);
  fh::nasdaq::PacketDecoder decoder;
  auto p = decoder.decode(hb.sent, hb.line, hb.bytes);
  REQUIRE(p.has_value());
  REQUIRE(p->kind == fh::PacketKind::Heartbeat);
  REQUIRE(p->next_seq == 42);

  const auto eos = fh::sim::make_end_of_session(6 * fh::kMillisecond, fh::Line::A, 43);
  p = decoder.decode(eos.sent, eos.line, eos.bytes);
  REQUIRE(p.has_value());
  REQUIRE(p->kind == fh::PacketKind::EndOfSession);
  REQUIRE(p->next_seq == 43);
}

TEST_CASE("a message with recorded bytes goes on the wire unchanged", "[sim][wire]") {
  fh::sim::GroundMessage m;
  m.seq = 1;
  m.filler = 'S';
  m.code = 'O';
  m.raw = {0x53, 0x00, 0x07, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x4F};
  REQUIRE(fh::sim::message_bytes(m) == m.raw);
  m.raw.clear();
  REQUIRE(fh::sim::message_bytes(m) != std::vector<std::uint8_t>{0x53, 0x00, 0x07});
}
