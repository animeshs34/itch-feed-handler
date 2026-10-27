#include "fh/sim/delivery.hpp"

#include <algorithm>

namespace fh::sim {

std::vector<Burst> make_bursts(const FaultConfig& faults, Nanos faulty_until, Nanos spacing,
                               fhtest::SplitMix64& rng) {
  std::vector<Burst> out;
  if (faults.burst_pct == 0 || faults.burst_max == 0) return out;
  for (Nanos t = spacing; t < faulty_until; t += spacing) {
    if (rng.below(100) >= faults.burst_pct) continue;
    const Nanos length = rng.between(1, static_cast<std::int64_t>(faults.burst_max)) * spacing;
    out.push_back(Burst{t, t + length});
    t += length;
  }
  return out;
}

std::vector<Arrival> deliver_line(const std::vector<Datagram>& datagrams, const FaultConfig& faults,
                                  Nanos faulty_until, const std::vector<Burst>& bursts,
                                  fhtest::SplitMix64& rng) {
  const auto in_burst = [&bursts](Nanos sent) {
    return std::any_of(bursts.begin(), bursts.end(),
                       [sent](const Burst& b) { return sent >= b.from && sent < b.to; });
  };
  const auto delay = [&]() { return faults.jitter > 0 ? rng.between(0, faults.jitter) : Nanos{0}; };

  std::vector<Arrival> out;
  for (const Datagram& d : datagrams) {
    const bool exposed = d.sent < faulty_until;
    if (in_burst(d.sent) || (exposed && rng.below(100) < faults.loss_pct)) continue;

    Arrival a;
    a.entry.t = d.sent + (d.line == Line::B ? faults.skew_b : 0) + delay();
    a.entry.kind = EntryKind::Main;
    a.entry.line = d.line;
    a.entry.bytes = d.bytes;
    a.dkind = d.kind;
    a.first_seq = d.first_seq;
    a.count = d.count;
    out.push_back(a);

    if (exposed && rng.below(100) < faults.dup_pct) {
      a.entry.t += kMicrosecond + delay();  // the second copy is always later
      out.push_back(a);
    }
  }
  return out;
}

}  // namespace fh::sim
