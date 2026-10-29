#include "fh/bench/workload.hpp"

#include "fh/sim/day.hpp"
#include "fh/sim/wire.hpp"
#include "prng.hpp"

namespace fh::bench {

Workload make_workload(std::size_t book_events, std::uint16_t instruments, std::uint64_t seed,
                       unsigned max_pack) {
  sim::DayConfig day_config;
  day_config.instruments = instruments;
  day_config.book_events = book_events;
  Workload w = make_workload(sim::make_day(seed, day_config), seed, max_pack);
  w.instruments = instruments;
  return w;
}

Workload make_workload(const sim::GroundTruth& truth, std::uint64_t seed, unsigned max_pack) {
  Workload w;
  w.seed = seed;
  w.instruments = truth.instruments().size();
  w.max_pack = max_pack;
  w.messages = truth.messages.size();

  fhtest::SplitMix64 rng(seed ^ 0xB3A7C1);
  const std::vector<Nanos> sent =
      sim::send_times(truth.messages.size(), 100 * kMicrosecond, rng, 0);
  const std::vector<sim::Datagram> datagrams = sim::packetize(truth, sent, Line::A, rng, max_pack);
  w.datagrams.reserve(datagrams.size());
  for (const sim::Datagram& d : datagrams) {
    w.first_seq.push_back(d.first_seq);
    w.bytes += d.bytes.size();
    w.datagrams.push_back(d.bytes);
  }
  return w;
}

}  // namespace fh::bench
