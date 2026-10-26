#include <catch2/catch_test_macros.hpp>

#include "event_gen.hpp"
#include "fh/nasdaq/itch_decoder.hpp"
#include "fh/nasdaq/itch_encoder.hpp"
#include "fh/nasdaq/itch_layout.hpp"

using namespace fh::nasdaq;

TEST_CASE("every filler decodes as a no-effect message of its type", "[itch_roundtrip]") {
  for (const MessageSpec& spec : kMessageSpecs) {
    if (spec.affects_book) continue;
    INFO("type " << spec.type);
    const ItchResult r = decode_itch(encode_filler(spec.type, ItchHeader{9, 0, 5}));
    REQUIRE(r.kind == ItchKind::NoEffect);
    REQUIRE(r.type == spec.type);
  }
}

TEST_CASE("system event, stock directory and trade are no-effect messages", "[itch_roundtrip]") {
  const ItchHeader h{2, 0, 0};
  REQUIRE(decode_itch(encode_system_event(h, 'Q')).kind == ItchKind::NoEffect);
  REQUIRE(decode_itch(encode_stock_directory(h, "MSFT")).kind == ItchKind::NoEffect);
  REQUIRE(decode_itch(encode_trade(h, 5, "MSFT", 100, 9)).kind == ItchKind::NoEffect);
}

TEST_CASE("an execution and a cancel decode to the same Reduce", "[itch_roundtrip]") {
  const fh::BookEvent reduce = fh::make_reduce(5, 3, 77, 10);
  EventStyle execute_style;
  execute_style.reduce_as_execute = true;
  execute_style.match_number = 99;
  EventStyle cancel_style;
  const ItchResult e = decode_itch(encode_event(reduce, execute_style));
  const ItchResult x = decode_itch(encode_event(reduce, cancel_style));
  REQUIRE(e.event == x.event);
  REQUIRE(e.event.shares == 10);
  REQUIRE(e.event.order == 77);
}

TEST_CASE("every generated event survives encode then decode", "[itch_roundtrip]") {
  for (std::uint64_t seed = 1; seed <= 100; ++seed) {
    const auto events = fhtest::generate_valid_events(seed, fhtest::GenConfig{5, 200});
    for (const fh::BookEvent& e : events) {
      EventStyle style;
      style.reduce_as_execute = (e.order % 2 == 0);
      style.match_number = e.seq;
      style.timestamp = e.seq * 1000;
      const ItchResult r = decode_itch(encode_event(e, style));
      INFO("seed " << seed << " seq " << e.seq);
      REQUIRE(r.kind == ItchKind::Event);
      fh::BookEvent expected = e;
      expected.seq = 0;  // the packet decoder supplies sequence numbers
      REQUIRE(r.event == expected);
    }
  }
}

TEST_CASE("the instrument, side, price and shares of an Add survive exactly", "[itch_roundtrip]") {
  for (const fh::Side side : {fh::Side::Buy, fh::Side::Sell}) {
    const fh::BookEvent e =
        fh::make_add(0, 65535, 0xFFFFFFFFFFFFFFFFULL, side, 2'000'000'000, 0xFFFFFFFF);
    const ItchResult r = decode_itch(encode_event(e, EventStyle{}));
    REQUIRE(r.kind == ItchKind::Event);
    REQUIRE(r.event == e);
  }
}
