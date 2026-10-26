#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "fh/nasdaq/handler.hpp"
#include "input_to_bytes.hpp"
#include "mini_sim.hpp"
#include "prng.hpp"

namespace {

// Damages a datagram in one of several ways.
fh::nasdaq::Bytes damage(fh::nasdaq::Bytes bytes, fhtest::SplitMix64& rng) {
  switch (rng.below(6)) {
    case 0:  // flip one byte
      if (!bytes.empty())
        bytes[rng.below(bytes.size())] ^= static_cast<std::uint8_t>(1 + rng.below(255));
      break;
    case 1:  // cut it short
      bytes.resize(rng.below(bytes.size() + 1));
      break;
    case 2:  // add bytes at the end
      for (std::uint64_t i = rng.below(8) + 1; i > 0; --i)
        bytes.push_back(static_cast<std::uint8_t>(rng.below(256)));
      break;
    case 3:  // overwrite a few bytes with random ones
      for (int k = 0; k < 4 && !bytes.empty(); ++k)
        bytes[rng.below(bytes.size())] = static_cast<std::uint8_t>(rng.below(256));
      break;
    case 4:  // change the count field (bytes 18 and 19)
      if (bytes.size() >= fh::nasdaq::kMoldHeaderBytes)
        bytes[19] = static_cast<std::uint8_t>(rng.below(256));
      break;
    default:  // pure noise of a random length
      bytes.assign(rng.below(80), 0);
      for (auto& b : bytes) b = static_cast<std::uint8_t>(rng.below(256));
      break;
  }
  return bytes;
}

}  // namespace

TEST_CASE("damaged datagrams never crash the decoders and every packet is accounted for",
          "[nasdaq][hostile]") {
  const auto main_session = fh::nasdaq::make_session("MAINSESS01");
  const auto snap_session = fh::nasdaq::make_session("SNAPSESS01");
  fhtest::SplitMix64 rng(31337);
  std::uint64_t accepted_main = 0;

  for (std::uint64_t seed = 1; seed <= 30; ++seed) {
    fhtest::MiniSimConfig sim;
    sim.seed = seed;
    sim.messages = 120;
    const fhtest::Scenario scenario = fhtest::make_scenario(sim);

    fh::nasdaq::PacketDecoder packets;
    fh::nasdaq::SnapshotStreamDecoder snapshots;
    std::uint64_t offered_main = 0, ok_main = 0, offered_snap = 0, ok_snap = 0;
    for (const fh::Input& input : scenario.inputs) {
      fhtest::ByteStep step = fhtest::to_byte_step(input, main_session, snap_session);
      if (step.kind == fhtest::ByteStep::Kind::Tick) continue;
      if (rng.below(3) == 0) step.bytes = damage(step.bytes, rng);
      if (step.kind == fhtest::ByteStep::Kind::Main) {
        ++offered_main;
        const auto p = packets.decode(step.t, step.line, step.bytes);
        if (p) {
          ++ok_main;
          // Whatever got through is internally consistent.
          for (std::size_t i = 1; i < p->messages.size(); ++i) {
            REQUIRE(p->messages[i].seq > p->messages[i - 1].seq);
          }
          for (const fh::SequencedMessage& m : p->messages) {
            if (m.event) REQUIRE(m.event->seq == m.seq);
          }
        }
      } else {
        ++offered_snap;
        ok_snap += snapshots.decode(step.t, step.bytes).has_value() ? 1U : 0U;
      }
    }

    std::uint64_t main_errors = 0, snap_errors = 0;
    for (const auto n : packets.stats().packet_errors) main_errors += n;
    for (const auto n : snapshots.stats().packet_errors) snap_errors += n;
    INFO("seed " << seed);
    REQUIRE(packets.stats().packets == offered_main);
    REQUIRE(ok_main + main_errors == offered_main);
    REQUIRE(snapshots.stats().packets == offered_snap);
    REQUIRE(ok_snap + snap_errors == offered_snap);
    accepted_main += ok_main;
  }
  REQUIRE(accepted_main > 0);
}

TEST_CASE("a whole run of damaged traffic through the handler never crashes", "[nasdaq][hostile]") {
  const auto main_session = fh::nasdaq::make_session("MAINSESS01");
  const auto snap_session = fh::nasdaq::make_session("SNAPSESS01");
  fhtest::SplitMix64 rng(99);
  for (std::uint64_t seed = 1; seed <= 20; ++seed) {
    fhtest::MiniSimConfig sim;
    sim.seed = 700 + seed;
    sim.messages = 150;
    const fhtest::Scenario scenario = fhtest::make_scenario(sim);
    fh::nasdaq::Handler handler{fh::CoreConfig{}};
    std::vector<fh::Output> out;
    for (const fh::Input& input : scenario.inputs) {
      fhtest::ByteStep step = fhtest::to_byte_step(input, main_session, snap_session);
      if (step.kind != fhtest::ByteStep::Kind::Tick && rng.below(4) == 0) {
        step.bytes = damage(step.bytes, rng);
      }
      switch (step.kind) {
        case fhtest::ByteStep::Kind::Main:
          handler.on_packet(step.t, step.line, step.bytes, out);
          break;
        case fhtest::ByteStep::Kind::Snapshot:
          handler.on_snapshot_packet(step.t, step.bytes, out);
          break;
        case fhtest::ByteStep::Kind::Tick:
          handler.on_tick(step.t, out);
          break;
      }
    }
  }
  SUCCEED("no crash, no sanitizer report");
}
