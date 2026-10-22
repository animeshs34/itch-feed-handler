#ifndef FH_TESTS_SUPPORT_MINI_SIM_HPP_
#define FH_TESTS_SUPPORT_MINI_SIM_HPP_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <set>
#include <utility>
#include <vector>

#include "event_gen.hpp"
#include "fh/messages.hpp"
#include "oracle_state.hpp"
#include "packets.hpp"
#include "prng.hpp"

namespace fhtest {

// A small stand-in for the full simulator (which comes later and works on raw
// bytes): it takes seeded ground truth and delivers it over two lossy, jittery
// lines plus a snapshot loop, as decoded inputs.
//
// Faults only hit the first part of the stream. The rest is clean, and the
// snapshot loop keeps running afterwards, so a correct core must end up fully
// recovered and equal to the oracle.
struct MiniSimConfig {
  std::uint64_t seed{1};
  std::size_t messages{400};
  std::uint16_t instruments{4};
  unsigned loss_pct{8};   // chance a line loses a message, in the faulty part
  unsigned burst_pct{2};  // chance a message starts a burst lost on BOTH lines
  unsigned dup_pct{5};    // chance a delivered message is delivered twice
  unsigned snapshot_loss_pct{10};
  unsigned snapshot_late_pct{15};
  unsigned faulty_pct{70};  // faults hit this percentage of the stream
  fh::Nanos spacing{10 * fh::kMicrosecond};
  fh::Nanos jitter{200 * fh::kMicrosecond};
  fh::Nanos skew_b{100 * fh::kMicrosecond};
  fh::Nanos heartbeat_period{500 * fh::kMicrosecond};
  fh::Nanos snapshot_period{2 * fh::kMillisecond};
  fh::Nanos tick_period{100 * fh::kMicrosecond};
  fh::Nanos clean_tail{30 * fh::kMillisecond};
};

struct Scenario {
  std::vector<fh::BookEvent> truth;
  std::vector<fh::Input> inputs;
};

inline fh::Nanos time_of(const fh::Input& input) {
  if (const auto* p = std::get_if<fh::PacketIn>(&input)) return p->t;
  if (const auto* s = std::get_if<fh::SnapshotPacketIn>(&input)) return s->t;
  return std::get<fh::TickIn>(input).t;
}

inline Scenario make_scenario(const MiniSimConfig& cfg) {
  SplitMix64 rng(cfg.seed);
  Scenario scenario;
  scenario.truth = generate_valid_events(rng.next(), GenConfig{cfg.instruments, cfg.messages});
  const auto& truth = scenario.truth;
  const std::size_t n = truth.size();

  struct Timed {
    fh::Nanos t;
    fh::Input input;
  };
  std::vector<Timed> items;
  const auto add = [&items](fh::Nanos t, fh::Input input) {
    items.emplace_back(t, std::move(input));
  };

  const std::size_t faulty_until = n * cfg.faulty_pct / 100;
  std::size_t burst_left = 0;
  fh::Nanos last_arrival = 0;
  const auto delay = [&]() { return rng.between(0, cfg.jitter); };

  for (std::size_t i = 0; i < n; ++i) {
    const fh::Nanos sent = static_cast<fh::Nanos>(i + 1) * cfg.spacing;
    const bool faulty = i < faulty_until;
    bool burst = false;
    if (faulty) {
      if (burst_left > 0) {
        burst = true;
        --burst_left;
      } else if (rng.below(100) < cfg.burst_pct) {
        burst = true;
        burst_left = rng.below(5);
      }
    }
    for (const fh::Line line : {fh::Line::A, fh::Line::B}) {
      const bool lose = burst || (faulty && rng.below(100) < cfg.loss_pct);
      if (lose) continue;
      const fh::Nanos arrive = sent + (line == fh::Line::B ? cfg.skew_b : 0) + delay();
      add(arrive, packet(line, arrive, {message(truth[i])}));
      last_arrival = std::max(last_arrival, arrive);
      if (faulty && rng.below(100) < cfg.dup_pct) {
        const fh::Nanos again = arrive + delay();
        add(again, packet(line, again, {message(truth[i])}));
        last_arrival = std::max(last_arrival, again);
      }
    }
  }

  const fh::Nanos end = last_arrival + cfg.clean_tail;
  const auto sent_by = [&](fh::Nanos t) {
    return static_cast<fh::Seq>(std::min<fh::Nanos>(static_cast<fh::Nanos>(n), t / cfg.spacing));
  };

  // Heartbeats on both lines, saying what the publisher will send next.
  for (fh::Nanos t = cfg.heartbeat_period; t <= end; t += cfg.heartbeat_period) {
    for (const fh::Line line : {fh::Line::A, fh::Line::B}) {
      add(t, heartbeat(line, t, sent_by(t) + 1));
    }
  }

  // The snapshot loop: one instrument per period, in turn.
  const std::set<fh::InstrumentId> inst_set = instruments_of(truth);
  const std::vector<fh::InstrumentId> insts(inst_set.begin(), inst_set.end());
  fh::Seq stream_seq = 1;
  std::size_t turn = 0;
  for (fh::Nanos t = cfg.snapshot_period; t <= end; t += cfg.snapshot_period, ++turn) {
    const fh::InstrumentId inst = insts[turn % insts.size()];
    const fh::Seq asof = sent_by(t);
    auto items_of_run = run_items(inst, asof, oracle_orders(truth, inst, asof));
    const fh::Seq first = stream_seq;
    stream_seq += items_of_run.size();
    const bool faulty = t < static_cast<fh::Nanos>(faulty_until) * cfg.spacing;
    if (faulty && rng.below(100) < cfg.snapshot_loss_pct) continue;  // lost on the way
    fh::Nanos arrive = t + rng.between(0, 3 * fh::kMillisecond);
    if (faulty && rng.below(100) < cfg.snapshot_late_pct)
      arrive += 6 * fh::kMillisecond;  // stale by arrival
    add(arrive, snapshot_packet(arrive, first, std::move(items_of_run)));
  }

  for (fh::Nanos t = cfg.tick_period; t <= end + 5 * fh::kMillisecond; t += cfg.tick_period) {
    add(t, tick(t));
  }

  // Sort positions, not items, so the inputs are moved once, into the scenario.
  std::vector<std::size_t> by_time(items.size());
  std::iota(by_time.begin(), by_time.end(), std::size_t{0});
  std::stable_sort(by_time.begin(), by_time.end(),
                   [&items](std::size_t a, std::size_t b) { return items[a].t < items[b].t; });
  for (const std::size_t i : by_time) scenario.inputs.push_back(std::move(items[i].input));
  return scenario;
}

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_MINI_SIM_HPP_
