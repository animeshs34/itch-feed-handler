#include <catch2/catch_test_macros.hpp>

#include "fh/core.hpp"
#include "packets.hpp"

// Found by the first campaign (plan 5): a snapshot reached the core before any main-feed
// packet, the main feed was then joined at a later sequence number, and the instrument,
// created live with an empty book, was fed events it could not apply.
TEST_CASE("joining the main feed late after an early snapshot does not corrupt the book",
          "[core][regression]") {
  fh::Core core{fh::CoreConfig{}};
  std::vector<fh::Output> out;

  // A snapshot of instrument 1, as of sequence 0 (nothing yet), before any main packet.
  core.step(fh::Input{fhtest::snapshot_packet(1000, 1, fhtest::run_items(1, 0, {}))}, out);

  // The main feed is joined at sequence 11: an add for instrument 1.
  const fh::BookEvent add = fh::make_add(11, 1, 500, fh::Side::Buy, 100, 10);
  core.step(fh::Input{fhtest::packet(fh::Line::A, 2000, {fhtest::message(add)})}, out);

  for (const fh::Output& o : out) {
    if (const auto* alarm = std::get_if<fh::Alarm>(&o)) {
      REQUIRE(alarm->kind != fh::AlarmKind::BookInconsistent);
    }
    if (const auto* update = std::get_if<fh::Update>(&o)) {
      REQUIRE(update->kind != fh::UpdateKind::Delta);  // nothing may be published yet
    }
  }
  REQUIRE(core.instruments().states().at(1).health() == fh::InstrumentHealth::Syncing);
  REQUIRE(core.counters().inconsistencies == 0);

  // A snapshot as of sequence 11 repairs it, and the add is covered by it.
  out.clear();
  const std::map<fh::OrderId, fh::Order> orders{{500, fh::Order{fh::Side::Buy, 100, 10}}};
  core.step(fh::Input{fhtest::snapshot_packet(3000, 3, fhtest::run_items(1, 11, orders))}, out);
  REQUIRE(core.instruments().states().at(1).health() == fh::InstrumentHealth::Live);
  REQUIRE(core.instruments().states().at(1).book().orders() == orders);
}
