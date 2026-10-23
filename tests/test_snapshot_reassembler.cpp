#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "fh/snapshot_reassembler.hpp"
#include "packets.hpp"

using fh::Order;
using fh::Side;

namespace {

std::map<fh::OrderId, Order> two_orders() {
  return {{101, Order{Side::Buy, 100, 10}}, {102, Order{Side::Sell, 101, 5}}};
}

std::vector<fh::SnapshotRun> accept(fh::SnapshotReassembler& r, const fh::SnapshotPacketIn& p) {
  std::vector<fh::SnapshotRun> completed;
  r.accept(p, completed);
  return completed;
}

}  // namespace

TEST_CASE("a whole run in one packet completes", "[reassembler]") {
  fh::SnapshotReassembler r;
  const auto done = accept(r, fhtest::snapshot_packet(0, 1, fhtest::run_items(1, 7, two_orders())));
  REQUIRE(done.size() == 1);
  REQUIRE(done[0].inst == 1);
  REQUIRE(done[0].asof == 7);
  REQUIRE(done[0].orders == two_orders());
  REQUIRE_FALSE(r.run_open());
}

TEST_CASE("a run split over contiguous packets completes at its last packet", "[reassembler]") {
  fh::SnapshotReassembler r;
  const auto items = fhtest::run_items(1, 7, two_orders());  // Open, Order, Order, Close
  const std::vector<fh::SnapshotItem> first(items.begin(), items.begin() + 2);
  const std::vector<fh::SnapshotItem> second(items.begin() + 2, items.end());
  REQUIRE(accept(r, fhtest::snapshot_packet(0, 1, first)).empty());
  REQUIRE(r.run_open());
  const auto done = accept(r, fhtest::snapshot_packet(0, 3, second));
  REQUIRE(done.size() == 1);
  REQUIRE(done[0].orders == two_orders());
}

TEST_CASE("a break in the stream numbering discards the run in progress", "[reassembler]") {
  fh::SnapshotReassembler r;
  const auto items = fhtest::run_items(1, 7, two_orders());
  const std::vector<fh::SnapshotItem> first(items.begin(), items.begin() + 2);
  const std::vector<fh::SnapshotItem> second(items.begin() + 2, items.end());
  REQUIRE(accept(r, fhtest::snapshot_packet(0, 1, first)).empty());
  REQUIRE(accept(r, fhtest::snapshot_packet(0, 9, second)).empty());  // 3 expected, 9 arrived
  REQUIRE(r.discarded_runs() == 1);
  REQUIRE_FALSE(r.run_open());

  // The next complete run still works.
  const auto done = accept(r, fhtest::snapshot_packet(0, 12, fhtest::run_items(2, 8, {})));
  REQUIRE(done.size() == 1);
  REQUIRE(done[0].inst == 2);
}

TEST_CASE("a new Open while a run is open discards the unfinished one", "[reassembler]") {
  fh::SnapshotReassembler r;
  std::vector<fh::SnapshotItem> items;
  items.emplace_back(fh::SnapshotOpen{1});
  items.emplace_back(fh::SnapshotOrder{1, 101, Order{Side::Buy, 100, 10}});
  for (auto& i : fhtest::run_items(2, 9, {})) items.push_back(i);  // Open 2, Close
  const auto done = accept(r, fhtest::snapshot_packet(0, 1, items));
  REQUIRE(done.size() == 1);
  REQUIRE(done[0].inst == 2);
  REQUIRE(r.discarded_runs() == 1);
}

TEST_CASE("orders before any Open and a Close without an Open are ignored", "[reassembler]") {
  fh::SnapshotReassembler r;
  std::vector<fh::SnapshotItem> items;
  items.emplace_back(fh::SnapshotOrder{1, 101, Order{Side::Buy, 100, 10}});
  items.emplace_back(fh::SnapshotClose{5});
  REQUIRE(accept(r, fhtest::snapshot_packet(0, 1, items)).empty());
  REQUIRE(r.discarded_runs() == 0);
}

TEST_CASE("an order for another instrument or a repeated id discards the run", "[reassembler]") {
  fh::SnapshotReassembler r;
  std::vector<fh::SnapshotItem> wrong_inst;
  wrong_inst.emplace_back(fh::SnapshotOpen{1});
  wrong_inst.emplace_back(fh::SnapshotOrder{2, 101, Order{Side::Buy, 100, 10}});
  wrong_inst.emplace_back(fh::SnapshotClose{5});
  REQUIRE(accept(r, fhtest::snapshot_packet(0, 1, wrong_inst)).empty());
  REQUIRE(r.discarded_runs() == 1);

  std::vector<fh::SnapshotItem> repeated;
  repeated.emplace_back(fh::SnapshotOpen{1});
  repeated.emplace_back(fh::SnapshotOrder{1, 101, Order{Side::Buy, 100, 10}});
  repeated.emplace_back(fh::SnapshotOrder{1, 101, Order{Side::Buy, 100, 10}});
  repeated.emplace_back(fh::SnapshotClose{5});
  REQUIRE(accept(r, fhtest::snapshot_packet(0, 4, repeated)).empty());
  REQUIRE(r.discarded_runs() == 2);
}

TEST_CASE("a run with no orders is valid", "[reassembler]") {
  fh::SnapshotReassembler r;
  const auto done = accept(r, fhtest::snapshot_packet(0, 1, fhtest::run_items(3, 4, {})));
  REQUIRE(done.size() == 1);
  REQUIRE(done[0].orders.empty());
  REQUIRE(done[0].asof == 4);
}

TEST_CASE("several runs in one packet all complete", "[reassembler]") {
  fh::SnapshotReassembler r;
  std::vector<fh::SnapshotItem> items = fhtest::run_items(1, 5, two_orders());
  for (auto& i : fhtest::run_items(2, 6, {})) items.push_back(i);
  const auto done = accept(r, fhtest::snapshot_packet(0, 1, items));
  REQUIRE(done.size() == 2);
  REQUIRE(done[0].inst == 1);
  REQUIRE(done[1].inst == 2);
}
