#include <catch2/catch_test_macros.hpp>
#include <cstdio>

#include "fh/sim/scenario.hpp"
#include "fh/sim/trace_io.hpp"

TEST_CASE("a trace survives encode and decode unchanged", "[sim][trace-io]") {
  for (unsigned tier = 0; tier < fh::sim::kTierCount; ++tier) {
    const auto trace =
        fh::sim::make_trace(3, fh::sim::scenario_config(static_cast<fh::sim::Tier>(tier)));
    const auto bytes = fh::sim::encode_trace(trace);
    const auto back = fh::sim::decode_trace(bytes);
    REQUIRE(back.has_value());
    REQUIRE(*back == trace);
  }
}

TEST_CASE("equal traces give equal bytes, different traces different bytes", "[sim][trace-io]") {
  const auto config = fh::sim::scenario_config(fh::sim::Tier::Moderate);
  REQUIRE(fh::sim::encode_trace(fh::sim::make_trace(4, config)) ==
          fh::sim::encode_trace(fh::sim::make_trace(4, config)));
  REQUIRE(fh::sim::encode_trace(fh::sim::make_trace(4, config)) !=
          fh::sim::encode_trace(fh::sim::make_trace(5, config)));
}

TEST_CASE("the encoding starts with the magic and version, little-endian", "[sim][trace-io]") {
  const auto bytes = fh::sim::encode_trace(fh::sim::Trace{});
  REQUIRE(bytes[0] == 'F');
  REQUIRE(bytes[1] == 'H');
  REQUIRE(bytes[2] == 'T');
  REQUIRE(bytes[3] == 'R');
  REQUIRE(bytes[4] == 2);
  REQUIRE(bytes[5] == 0);
}

TEST_CASE("an empty trace round-trips", "[sim][trace-io]") {
  const auto back = fh::sim::decode_trace(fh::sim::encode_trace(fh::sim::Trace{}));
  REQUIRE(back.has_value());
  REQUIRE(*back == fh::sim::Trace{});
}

TEST_CASE("every truncation of an encoding is refused", "[sim][trace-io]") {
  auto config = fh::sim::scenario_config(fh::sim::Tier::Clean);
  config.day.book_events = 5;
  config.timing.tail = 2 * fh::kMillisecond;
  const auto bytes = fh::sim::encode_trace(fh::sim::make_trace(1, config));
  REQUIRE(bytes.size() > 500);
  // Every prefix, short of the whole thing, is refused. (Stepping keeps the test fast.)
  for (std::size_t n = 0; n < bytes.size(); n += (n < 400 ? 1 : 97)) {
    REQUIRE_FALSE(fh::sim::decode_trace(std::span(bytes).first(n)).has_value());
  }
  REQUIRE(fh::sim::decode_trace(bytes).has_value());
}

TEST_CASE("trailing bytes, a wrong magic and a wrong version are refused", "[sim][trace-io]") {
  auto bytes = fh::sim::encode_trace(fh::sim::Trace{});
  auto extra = bytes;
  extra.push_back(0);
  REQUIRE_FALSE(fh::sim::decode_trace(extra).has_value());
  auto bad_magic = bytes;
  bad_magic[0] = 'X';
  REQUIRE_FALSE(fh::sim::decode_trace(bad_magic).has_value());
  auto bad_version = bytes;
  bad_version[4] = 9;
  REQUIRE_FALSE(fh::sim::decode_trace(bad_version).has_value());
}

TEST_CASE("a count larger than the bytes that remain is refused without allocating",
          "[sim][trace-io]") {
  auto bytes = fh::sim::encode_trace(fh::sim::Trace{});
  // The message count sits after magic(4) version(2) seed(8) core(8+8+8+8+8) = 54 bytes.
  bytes[54] = 0xFF;
  bytes[55] = 0xFF;
  bytes[56] = 0xFF;
  bytes[57] = 0xFF;
  REQUIRE_FALSE(fh::sim::decode_trace(bytes).has_value());
}

TEST_CASE("out-of-range enum values are refused", "[sim][trace-io]") {
  fh::sim::Trace t;
  t.entries.push_back(fh::sim::Entry{5, fh::sim::EntryKind::Tick, fh::Line::A, {}});
  auto bytes = fh::sim::encode_trace(t);
  // The last entry is: t(8) kind(1) line(1) length(4) = 14 bytes at the end.
  auto bad_kind = bytes;
  bad_kind[bytes.size() - 6] = 9;
  REQUIRE_FALSE(fh::sim::decode_trace(bad_kind).has_value());
  auto bad_line = bytes;
  bad_line[bytes.size() - 5] = 2;
  REQUIRE_FALSE(fh::sim::decode_trace(bad_line).has_value());
}

TEST_CASE("a trace written to a file and read back is the same", "[sim][trace-io]") {
  const auto trace = fh::sim::make_trace(2, fh::sim::scenario_config(fh::sim::Tier::Mild));
  const std::string path = "trace_io_test.fhtr";
  REQUIRE(fh::sim::write_trace_file(path, trace));
  const auto back = fh::sim::read_trace_file(path);
  std::remove(path.c_str());
  REQUIRE(back.has_value());
  REQUIRE(*back == trace);
  REQUIRE_FALSE(fh::sim::read_trace_file("no/such/file.fhtr").has_value());
}

TEST_CASE("recorded bytes inside the true messages survive the round trip", "[sim][trace-io]") {
  fh::sim::Trace t;
  fh::sim::GroundMessage m;
  m.seq = 1;
  m.filler = 'S';
  m.code = 'O';
  m.raw = {0x53, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x4F};
  t.truth.messages.push_back(m);
  const auto back = fh::sim::decode_trace(fh::sim::encode_trace(t));
  REQUIRE(back.has_value());
  REQUIRE(*back == t);
  REQUIRE(back->truth.messages[0].raw == m.raw);
}

TEST_CASE("a version 1 trace is still read, with no recorded bytes", "[sim][trace-io]") {
  // Build a version 2 encoding of one filler message, then turn it into version 1 by
  // setting the version and removing the 4-byte raw length that follows the locate.
  fh::sim::Trace t;
  fh::sim::GroundMessage m;
  m.seq = 1;
  m.filler = 'S';
  m.code = 'O';
  t.truth.messages.push_back(m);
  auto bytes = fh::sim::encode_trace(t);
  bytes[4] = 1;
  const std::size_t raw_length_at =
      54 + 4 + 8 + 1 + 1 + 1 + 2;  // header, count, message up to locate
  bytes.erase(bytes.begin() + static_cast<std::ptrdiff_t>(raw_length_at),
              bytes.begin() + static_cast<std::ptrdiff_t>(raw_length_at + 4));
  const auto back = fh::sim::decode_trace(bytes);
  REQUIRE(back.has_value());
  REQUIRE(back->truth.messages.size() == 1);
  REQUIRE(back->truth.messages[0].filler == 'S');
  REQUIRE(back->truth.messages[0].raw.empty());
}

TEST_CASE("a version 3 trace is refused", "[sim][trace-io]") {
  auto bytes = fh::sim::encode_trace(fh::sim::Trace{});
  bytes[4] = 3;
  REQUIRE_FALSE(fh::sim::decode_trace(bytes).has_value());
}

TEST_CASE("a raw length larger than the bytes that remain is refused", "[sim][trace-io]") {
  fh::sim::Trace t;
  fh::sim::GroundMessage m;
  m.seq = 1;
  m.filler = 'N';
  t.truth.messages.push_back(m);
  auto bytes = fh::sim::encode_trace(t);
  const std::size_t raw_length_at = 54 + 4 + 8 + 1 + 1 + 1 + 2;
  bytes[raw_length_at + 3] = 0x7F;  // a length of about two billion
  REQUIRE_FALSE(fh::sim::decode_trace(bytes).has_value());
}
