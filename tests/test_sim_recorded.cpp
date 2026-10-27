#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "fh/nasdaq/binary_file.hpp"
#include "fh/nasdaq/itch_encoder.hpp"
#include "fh/sim/consistency.hpp"
#include "fh/sim/day.hpp"
#include "fh/sim/recorded.hpp"
#include "fh/sim/wire.hpp"

namespace {

// A BinaryFILE image of a synthetic day, as if it had been recorded.
std::string file_of(const fh::sim::GroundTruth& day, bool terminator = true) {
  std::stringstream out;
  fh::nasdaq::BinaryFileWriter writer(out);
  for (const auto& m : day.messages) REQUIRE(writer.write(fh::sim::message_bytes(m)));
  if (terminator) REQUIRE(writer.write_end_of_session());
  return out.str();
}

fh::sim::LoadResult load(const std::string& file, const fh::sim::LoadOptions& options = {}) {
  std::istringstream in(file);
  return fh::sim::load_recorded(in, options);
}

}  // namespace

TEST_CASE("a recorded day loads into the same book events and keeps its exact bytes",
          "[sim][recorded]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(1, fh::sim::DayConfig{});
  const auto result = load(file_of(day));
  REQUIRE(result.ok());
  REQUIRE(result.truth.messages.size() == day.messages.size());
  REQUIRE(result.truth.events() == day.events());
  for (std::size_t i = 0; i < day.messages.size(); ++i) {
    REQUIRE(result.truth.messages[i].raw == fh::sim::message_bytes(day.messages[i]));
    REQUIRE(result.truth.messages[i].seq == i + 1);
  }
  REQUIRE(result.census.records == day.messages.size());
  REQUIRE(result.census.kept == day.messages.size());
  REQUIRE(result.census.book_events == day.events().size());
}

TEST_CASE("fillers keep their type letter, their locate and the system event code",
          "[sim][recorded]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(2, fh::sim::DayConfig{});
  const auto result = load(file_of(day));
  REQUIRE(result.ok());
  for (std::size_t i = 0; i < day.messages.size(); ++i) {
    if (day.messages[i].event) continue;
    REQUIRE(result.truth.messages[i].filler == day.messages[i].filler);
    REQUIRE(result.truth.messages[i].locate == day.messages[i].locate);
    if (day.messages[i].filler == 'S')
      REQUIRE(result.truth.messages[i].code == day.messages[i].code);
  }
}

TEST_CASE("the census counts messages by type and by instrument", "[sim][recorded]") {
  fh::sim::DayConfig config;
  config.instruments = 3;
  config.book_events = 60;
  config.filler_every = 0;
  const fh::sim::GroundTruth day = fh::sim::make_day(3, config);
  const auto result = load(file_of(day));
  const auto& c = result.census;
  REQUIRE(c.by_type['S'] == 6);  // S/O S/S S/Q S/M S/E S/C
  REQUIRE(c.by_type['R'] == 3);
  REQUIRE(c.by_type['H'] == 3);
  REQUIRE(c.by_type['Y'] == 3);
  const auto add_like = c.by_type['A'];
  REQUIRE(add_like > 0);
  REQUIRE(c.by_type['A'] + c.by_type['E'] + c.by_type['X'] + c.by_type['D'] + c.by_type['U'] == 60);
  std::uint64_t total = 0;
  for (const auto& [inst, n] : c.by_instrument) total += n;
  REQUIRE(total == c.records);
  REQUIRE(c.by_instrument.at(0) == 6);  // the market-wide messages
  REQUIRE(c.unknown_types == 0);
}

TEST_CASE("keeping only some instruments keeps their messages and the market-wide ones",
          "[sim][recorded]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(4, fh::sim::DayConfig{});
  fh::sim::LoadOptions options;
  options.only = {2};
  const auto result = load(file_of(day), options);
  REQUIRE(result.ok());
  REQUIRE(result.census.records == day.messages.size());  // everything was read ...
  REQUIRE(result.census.kept < day.messages.size());      // ... but only some kept
  for (std::size_t i = 0; i < result.truth.messages.size(); ++i) {
    const auto& m = result.truth.messages[i];
    REQUIRE(m.seq == i + 1);
    REQUIRE((m.locate == 0 || m.locate == 2));
    if (m.event) {
      REQUIRE(m.event->inst == 2);
      REQUIRE(m.event->seq == m.seq);
    }
  }
}

TEST_CASE("a slice that keeps the start of the day stays consistent", "[sim][recorded]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(5, fh::sim::DayConfig{});
  fh::sim::LoadOptions options;
  options.only = {1, 3};
  const auto result = load(file_of(day), options);
  REQUIRE(result.ok());
  REQUIRE(fh::sim::check_consistency(result.truth).ok());
}

TEST_CASE("a limit keeps the first messages and stops reading", "[sim][recorded]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(6, fh::sim::DayConfig{});
  fh::sim::LoadOptions options;
  options.limit = 100;
  const auto result = load(file_of(day, /*terminator=*/false), options);
  REQUIRE(result.ok());  // a prefix is fine even without the terminator
  REQUIRE(result.truth.messages.size() == 100);
  REQUIRE(result.census.records == 100);
  REQUIRE(fh::sim::check_consistency(result.truth).ok());
  for (std::size_t i = 0; i < 100; ++i) {
    REQUIRE(result.truth.messages[i].raw == fh::sim::message_bytes(day.messages[i]));
  }
}

TEST_CASE("a file without the end-of-session record is incomplete unless allowed",
          "[sim][recorded]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(7, fh::sim::DayConfig{});
  const std::string file = file_of(day, false);
  REQUIRE(load(file).status == fh::sim::LoadStatus::Incomplete);
  fh::sim::LoadOptions options;
  options.allow_incomplete = true;
  REQUIRE(load(file, options).ok());
}

TEST_CASE("a file cut inside a record is truncated", "[sim][recorded]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(8, fh::sim::DayConfig{});
  std::string file = file_of(day);
  file.resize(file.size() - 5);  // inside the last message or the terminator
  const auto status = load(file).status;
  REQUIRE((status == fh::sim::LoadStatus::Truncated || status == fh::sim::LoadStatus::Incomplete));
  std::string cut = file_of(day);
  cut.resize(cut.size() / 2 + 1);
  REQUIRE(load(cut).status != fh::sim::LoadStatus::Ok);
}

TEST_CASE("bytes after the terminator are refused", "[sim][recorded]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(9, fh::sim::DayConfig{});
  REQUIRE(load(file_of(day) + "x").status == fh::sim::LoadStatus::TrailingBytes);
}

TEST_CASE("a message the decoder refuses stops the load and says which", "[sim][recorded]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(10, fh::sim::DayConfig{});
  std::stringstream out;
  fh::nasdaq::BinaryFileWriter writer(out);
  for (std::size_t i = 0; i < 5; ++i)
    REQUIRE(writer.write(fh::sim::message_bytes(day.messages[i])));
  // An add order whose side byte is neither B nor S.
  fh::nasdaq::Bytes add =
      fh::nasdaq::encode_add(fh::nasdaq::ItchHeader{1, 0, 0}, 5, fh::Side::Buy, 10, "I1", 100);
  add[19] = 'Z';  // the buy/sell indicator is the byte at offset 19 in an Add Order message
  REQUIRE(writer.write(add));
  REQUIRE(writer.write_end_of_session());

  const auto result = load(out.str());
  REQUIRE(result.status == fh::sim::LoadStatus::BadMessage);
  REQUIRE(result.record == 6);
  REQUIRE(result.error == fh::nasdaq::ItchError::BadSide);
}

TEST_CASE("an empty stream and a stream of only the terminator are empty", "[sim][recorded]") {
  REQUIRE(load("").status == fh::sim::LoadStatus::Empty);
  REQUIRE(load(std::string("\x00\x00", 2)).status == fh::sim::LoadStatus::Empty);
}

TEST_CASE("unknown message types are kept as fillers and counted", "[sim][recorded]") {
  std::stringstream out;
  fh::nasdaq::BinaryFileWriter writer(out);
  const fh::nasdaq::Bytes unknown = {'#',  0x00, 0x05, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x01, 0x02};
  REQUIRE(writer.write(unknown));
  REQUIRE(writer.write_end_of_session());
  const auto result = load(out.str());
  REQUIRE(result.ok());
  REQUIRE(result.census.unknown_types == 1);
  REQUIRE(result.truth.messages.size() == 1);
  REQUIRE(result.truth.messages[0].filler == '#');
  REQUIRE(result.truth.messages[0].locate == 5);
  REQUIRE(result.truth.messages[0].raw == unknown);
}

TEST_CASE("the status has a readable name", "[sim][recorded]") {
  REQUIRE(fh::sim::to_string(fh::sim::LoadStatus::Ok) == "ok");
  REQUIRE(fh::sim::to_string(fh::sim::LoadStatus::Incomplete).starts_with("incomplete"));
}
