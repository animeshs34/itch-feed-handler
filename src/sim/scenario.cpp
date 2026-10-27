#include "fh/sim/scenario.hpp"

#include <algorithm>
#include <utility>

#include "fh/sim/delivery.hpp"
#include "fh/sim/main_faults.hpp"
#include "fh/sim/snapshot_faults.hpp"

namespace fh::sim {
namespace {

// The longest an arrival can be delayed beyond its send time: jitter, skew, the extra
// time a duplicate or a late snapshot takes, and the 3 ms snapshot transit.
Nanos worst_delay(const FaultConfig& f) { return f.jitter * 2 + f.skew_b + 12 * kMillisecond; }

}  // namespace

ScenarioConfig scenario_config(Tier tier) {
  ScenarioConfig config;
  config.faults = tier_config(tier);
  return config;
}

Trace make_trace(std::uint64_t seed, const ScenarioConfig& config) {
  // The first stream of the seed makes the synthetic day; make_trace_for takes the rest.
  fhtest::SplitMix64 master(seed);
  fhtest::SplitMix64 day_rng(master.next());
  return make_trace_for(make_day(day_rng.next(), config.day), seed, config);
}

Trace make_trace_for(const GroundTruth& truth, std::uint64_t seed, const ScenarioConfig& config) {
  const Timing& timing = config.timing;
  const FaultConfig& faults = config.faults;

  // One stream per purpose, always drawn in the same order. The first is the one a
  // synthetic day would use; it is drawn and dropped so that every other stream is the
  // same whether the day is synthetic or recorded.
  fhtest::SplitMix64 master(seed);
  (void)master.next();
  fhtest::SplitMix64 time_rng(master.next()), pack_a(master.next()), pack_b(master.next()),
      burst_rng(master.next()), deliver_a(master.next()), deliver_b(master.next()),
      silence_rng(master.next()), malformed_rng(master.next()), mismatch_rng(master.next()),
      session_rng(master.next()), snapshot_rng(master.next()), end_rng(master.next());

  Trace trace;
  trace.seed = seed;
  trace.core = faults.core;
  trace.truth = truth;
  if (truth.messages.empty()) return trace;
  const std::size_t n = trace.truth.messages.size();
  const std::size_t instruments = trace.truth.instruments().size();

  const std::vector<Nanos> sent =
      send_times(n, timing.spacing, time_rng, faults.rate_burst_permille);
  const Nanos last_sent = sent.back();
  const Nanos faulty_until = last_sent * static_cast<Nanos>(faults.faulty_pct) / 100;
  // Long enough for every instrument to get a snapshot at least twice after the faults stop.
  const Nanos tail =
      std::max(timing.tail, static_cast<Nanos>(3 * instruments) * timing.snapshot_period);
  const Nanos end = last_sent + tail + worst_delay(faults);

  const auto sent_by = [&sent](Nanos t) {
    return static_cast<Seq>(std::upper_bound(sent.begin(), sent.end(), t) - sent.begin());
  };

  // Both lines: the messages, a heartbeat every period, and the end of the session.
  const auto line_datagrams = [&](Line line, fhtest::SplitMix64& pack_rng) {
    std::vector<Datagram> out = packetize(trace.truth, sent, line, pack_rng, 3);
    for (Nanos t = timing.heartbeat_period; t <= end; t += timing.heartbeat_period) {
      out.push_back(make_heartbeat(t, line, sent_by(t) + 1));
    }
    out.push_back(make_end_of_session(last_sent + timing.spacing, line, n + 1));
    return out;
  };

  std::vector<Burst> bursts = make_bursts(faults, faulty_until, timing.spacing, burst_rng);
  // The end of the day can be lost on both lines. Nothing later arrives to show that
  // messages are missing except the heartbeats and the end-of-session packet, so this
  // is what exercises tail-gap detection.
  if (faults.end_loss_max != 0 && end_rng.below(100) < faults.end_loss_pct) {
    const Nanos length =
        end_rng.between(1, static_cast<std::int64_t>(faults.end_loss_max)) * timing.spacing;
    bursts.push_back(Burst{last_sent - length + 1, last_sent + 1});
  }
  std::vector<Arrival> arrivals =
      deliver_line(line_datagrams(Line::A, pack_a), faults, faulty_until, bursts, deliver_a);
  {
    std::vector<Arrival> b =
        deliver_line(line_datagrams(Line::B, pack_b), faults, faulty_until, bursts, deliver_b);
    arrivals.insert(arrivals.end(), b.begin(), b.end());
  }
  apply_silence(arrivals, faults, faulty_until, timing.spacing, silence_rng);
  apply_malformed(arrivals, faults, faulty_until, malformed_rng);
  add_mismatches(arrivals, faults, faulty_until, mismatch_rng);
  add_wrong_session(arrivals, faults, faulty_until, session_rng);

  {
    std::vector<Arrival> snaps =
        deliver_snapshots(trace.truth, sent, timing, faults, faulty_until, end, snapshot_rng);
    arrivals.insert(arrivals.end(), snaps.begin(), snaps.end());
  }

  // Time passing: the handler is told at every tick, even when nothing arrives.
  for (Nanos t = timing.tick_period; t <= end + 5 * kMillisecond; t += timing.tick_period) {
    Arrival tick;
    tick.entry.t = t;
    tick.entry.kind = EntryKind::Tick;
    arrivals.push_back(std::move(tick));
  }

  // Arrival order. Ties keep the order the entries were made in, and ticks were made last,
  // so a tick at the same instant as a datagram comes after it.
  std::stable_sort(arrivals.begin(), arrivals.end(),
                   [](const Arrival& a, const Arrival& b) { return a.entry.t < b.entry.t; });
  trace.entries.reserve(arrivals.size());
  for (Arrival& a : arrivals) trace.entries.push_back(std::move(a.entry));
  return trace;
}

}  // namespace fh::sim
