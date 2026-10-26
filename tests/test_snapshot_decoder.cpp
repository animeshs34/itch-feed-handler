#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <vector>

#include "byte_truth.hpp"
#include "fh/nasdaq/snapshot_decoder.hpp"
#include "hex.hpp"

using namespace fh::nasdaq;

namespace {

const SessionId kSnapSession = make_session("SNAPSESS01");

const std::map<fh::OrderId, fh::Order> kOrders = {{101, fh::Order{fh::Side::Buy, 100, 10}},
                                                  {102, fh::Order{fh::Side::Sell, 101, 5}}};

Bytes run_packet(fh::Seq stream_seq, fh::InstrumentId inst, fh::Seq asof) {
  return encode_packet(kSnapSession, stream_seq, fhtest::snapshot_messages(inst, asof, kOrders));
}

}  // namespace

TEST_CASE("one run decodes to Open, Orders and Close with the as-of one below G",
          "[snapshot_decoder]") {
  SnapshotStreamDecoder d;
  const auto p = d.decode(7, run_packet(1, 3, 40));  // G carries 41
  REQUIRE(p.has_value());
  REQUIRE(p->t == 7);
  REQUIRE(p->stream_seq == 1);
  REQUIRE(p->items.size() == 4);
  REQUIRE(std::get<fh::SnapshotOpen>(p->items[0]).inst == 3);
  const auto& first = std::get<fh::SnapshotOrder>(p->items[1]);
  REQUIRE(first.inst == 3);
  REQUIRE(first.id == 101);
  REQUIRE(first.order == fh::Order{fh::Side::Buy, 100, 10});
  const auto& second = std::get<fh::SnapshotOrder>(p->items[2]);
  REQUIRE(second.id == 102);
  REQUIRE(std::get<fh::SnapshotClose>(p->items[3]).asof == 40);
}

TEST_CASE("an empty book is Open then Close", "[snapshot_decoder]") {
  SnapshotStreamDecoder d;
  const auto messages = fhtest::snapshot_messages(5, 9, {});
  const auto p = d.decode(0, encode_packet(kSnapSession, 1, messages));
  REQUIRE(p.has_value());
  REQUIRE(p->items.size() == 2);
}

TEST_CASE("a heartbeat on the snapshot stream carries only its numbering", "[snapshot_decoder]") {
  SnapshotStreamDecoder d;
  const auto p = d.decode(0, encode_heartbeat(kSnapSession, 12));
  REQUIRE(p.has_value());
  REQUIRE(p->stream_seq == 12);
  REQUIRE(p->items.empty());
}

TEST_CASE("every message is exactly one item so numbering stays in step", "[snapshot_decoder]") {
  SnapshotStreamDecoder d;
  const auto p = d.decode(0, run_packet(10, 1, 5));
  REQUIRE(p.has_value());
  REQUIRE(p->items.size() == 4);  // the next packet must be numbered 14
}

TEST_CASE("a message the snapshot stream never carries drops the packet", "[snapshot_decoder]") {
  SnapshotStreamDecoder d;
  std::vector<Bytes> messages = fhtest::snapshot_messages(1, 5, kOrders);
  messages.insert(messages.begin() + 1, encode_system_event(ItchHeader{}, 'O'));
  REQUIRE_FALSE(d.decode(0, encode_packet(kSnapSession, 1, messages)).has_value());
  REQUIRE(d.stats().packet_errors[static_cast<std::size_t>(PacketError::UnexpectedMessage)] == 1);

  const std::vector<Bytes> delete_msg = {encode_delete(ItchHeader{1, 0, 0}, 5)};
  REQUIRE_FALSE(d.decode(0, encode_packet(kSnapSession, 9, delete_msg)).has_value());
}

TEST_CASE("an invalid order inside the run drops the packet", "[snapshot_decoder]") {
  SnapshotStreamDecoder d;
  auto messages = fhtest::snapshot_messages(1, 5, kOrders);
  messages[1][19] = 'X';  // bad side in the first A
  REQUIRE_FALSE(d.decode(0, encode_packet(kSnapSession, 1, messages)).has_value());
}

TEST_CASE("a bad end-of-snapshot message drops the packet", "[snapshot_decoder]") {
  SnapshotStreamDecoder d;
  auto messages = fhtest::snapshot_messages(1, 5, kOrders);
  Bytes& g = messages.back();

  Bytes wrong_length = g;
  wrong_length.pop_back();
  auto m1 = messages;
  m1.back() = wrong_length;
  REQUIRE_FALSE(d.decode(0, encode_packet(kSnapSession, 1, m1)).has_value());

  Bytes letters = g;
  letters[20] = 'x';
  auto m2 = messages;
  m2.back() = letters;
  REQUIRE_FALSE(d.decode(0, encode_packet(kSnapSession, 1, m2)).has_value());

  auto m3 = messages;
  m3.back() = encode_end_of_snapshot(0);  // would mean as-of -1
  REQUIRE_FALSE(d.decode(0, encode_packet(kSnapSession, 1, m3)).has_value());
}

TEST_CASE("another session on the snapshot stream is dropped", "[snapshot_decoder]") {
  SnapshotStreamDecoder d;
  REQUIRE(d.decode(0, run_packet(1, 1, 5)).has_value());
  const auto other = encode_packet(make_session("ELSE"), 5, fhtest::snapshot_messages(1, 5, {}));
  REQUIRE_FALSE(d.decode(0, other).has_value());
  REQUIRE(d.stats().packet_errors[static_cast<std::size_t>(PacketError::SessionMismatch)] == 1);
}

TEST_CASE("a damaged packet does not set the session", "[snapshot_decoder]") {
  SnapshotStreamDecoder d;
  Bytes bad = run_packet(1, 1, 5);
  bad.pop_back();
  REQUIRE_FALSE(d.decode(0, bad).has_value());
  const auto other = encode_packet(make_session("ELSE"), 1, fhtest::snapshot_messages(1, 5, {}));
  REQUIRE(d.decode(0, other).has_value());  // this one names the session instead
}

TEST_CASE("the resume number parses spaces then digits and nothing else", "[snapshot_decoder]") {
  const auto num = [](const char* text) {
    const Bytes b(text, text + 20);
    return parse_resume_seq(b);
  };
  REQUIRE(num("                   8") == 8);
  REQUIRE(num("00000000000000000042") == 42);
  REQUIRE(num("18446744073709551615") == std::numeric_limits<std::uint64_t>::max());
  REQUIRE_FALSE(num("18446744073709551616").has_value());  // one more than fits
  REQUIRE_FALSE(num("99999999999999999999").has_value());
  REQUIRE_FALSE(num("                    ").has_value());  // no digits
  REQUIRE_FALSE(num("                  1x").has_value());
  REQUIRE_FALSE(num("                 1 2").has_value());  // space inside
  REQUIRE_FALSE(num("                  -1").has_value());
}
