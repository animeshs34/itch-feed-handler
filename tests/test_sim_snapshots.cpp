#include <catch2/catch_test_macros.hpp>
#include <string>

#include "fh/nasdaq/snapshot_decoder.hpp"
#include "fh/sim/day.hpp"
#include "fh/sim/snapshots.hpp"

namespace {

// Decodes a whole run through the real snapshot decoder and returns its items in order.
std::vector<fh::SnapshotItem> decode_run(const std::vector<fh::sim::SnapshotDatagram>& run) {
  fh::nasdaq::SnapshotStreamDecoder decoder;
  std::vector<fh::SnapshotItem> items;
  for (const auto& d : run) {
    auto packet = decoder.decode(d.sent, d.bytes);
    REQUIRE(packet.has_value());
    REQUIRE(packet->stream_seq == d.first_stream_seq);
    for (auto& item : packet->items) items.push_back(item);
  }
  return items;
}

// The items have no operator==, so compare them as text.
std::vector<std::string> as_text(const std::vector<fh::SnapshotItem>& items) {
  std::vector<std::string> out;
  for (const auto& item : items) {
    if (const auto* open = std::get_if<fh::SnapshotOpen>(&item)) {
      out.push_back("open " + std::to_string(open->inst));
    } else if (const auto* o = std::get_if<fh::SnapshotOrder>(&item)) {
      out.push_back("order " + std::to_string(o->inst) + " " + std::to_string(o->id) + " " +
                    std::to_string(o->order.price) + " " + std::to_string(o->order.shares));
    } else {
      out.push_back("close " + std::to_string(std::get<fh::SnapshotClose>(item).asof));
    }
  }
  return out;
}

}  // namespace

TEST_CASE("a run is the stock directory, the live orders and the close", "[sim][snapshot]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(1, fh::sim::DayConfig{});
  fh::sim::SnapshotBuilder builder(day);
  const fh::Seq asof = day.last_seq() / 2;
  const auto run = builder.build_run(5 * fh::kMillisecond, 2, asof, 1000);

  REQUIRE(run.size() == 1);
  const auto items = decode_run(run);
  REQUIRE(std::get<fh::SnapshotOpen>(items.front()).inst == 2);
  REQUIRE(std::get<fh::SnapshotClose>(items.back()).asof == asof);

  const auto expected = fhtest::oracle_orders(day.events(), 2, asof);
  std::map<fh::OrderId, fh::Order> got;
  for (std::size_t i = 1; i + 1 < items.size(); ++i) {
    const auto& o = std::get<fh::SnapshotOrder>(items[i]);
    REQUIRE(o.inst == 2);
    got.emplace(o.id, o.order);
  }
  REQUIRE(got == expected);
  REQUIRE(items.size() == expected.size() + 2);
}

TEST_CASE("a long run is cut into datagrams that join up exactly", "[sim][snapshot]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(2, fh::sim::DayConfig{});
  fh::sim::SnapshotBuilder builder(day);
  const auto whole = [&] {
    fh::sim::SnapshotBuilder b(day);
    return decode_run(b.build_run(0, 1, day.last_seq(), 1000));
  }();
  REQUIRE(whole.size() > 6);  // the test means little if the run is tiny

  const auto cut = builder.build_run(0, 1, day.last_seq(), 3);
  REQUIRE(cut.size() > 1);
  fh::Seq expected_first = 1;
  for (std::size_t i = 0; i < cut.size(); ++i) {
    REQUIRE(cut[i].index == i);
    REQUIRE(cut[i].of == cut.size());
    REQUIRE(cut[i].count <= 3);
    REQUIRE(cut[i].first_stream_seq == expected_first);
    expected_first += cut[i].count;
  }
  REQUIRE(as_text(decode_run(cut)) == as_text(whole));
}

TEST_CASE("the snapshot stream's numbering runs on from run to run", "[sim][snapshot]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(3, fh::sim::DayConfig{});
  fh::sim::SnapshotBuilder builder(day);
  REQUIRE(builder.next_stream_seq() == 1);
  const auto first = builder.build_run(0, 1, 20, 1000);
  const fh::Seq after_first = builder.next_stream_seq();
  REQUIRE(after_first == 1U + first.front().count);
  const auto second = builder.build_run(0, 2, 40, 1000);
  REQUIRE(second.front().first_stream_seq == after_first);
  REQUIRE(second.front().run == 1);
  REQUIRE(first.front().run == 0);
}

TEST_CASE("a snapshot of a still-empty book is just the directory and the close",
          "[sim][snapshot]") {
  const fh::sim::GroundTruth day = fh::sim::make_day(4, fh::sim::DayConfig{});
  fh::sim::SnapshotBuilder builder(day);
  const auto items = decode_run(builder.build_run(0, 1, 0, 1000));
  REQUIRE(items.size() == 2);
}
