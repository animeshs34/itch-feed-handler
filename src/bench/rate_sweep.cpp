#include "fh/bench/rate_sweep.hpp"

#include <vector>

#include "fh/core.hpp"
#include "fh/nasdaq/handler.hpp"
#include "fh/nasdaq/moldudp64.hpp"

namespace fh::bench {

OpenLoopResult bench_open_loop(const Workload& workload, Stage stage, double datagrams_per_second,
                               std::uint64_t count, std::uint64_t warmup) {
  OpenLoopConfig config;
  config.count = count;
  config.warmup = warmup;
  config.period = static_cast<Nanos>(static_cast<double>(kSecond) / datagrams_per_second);

  const ClockFn now = monotonic_clock();
  const WaitFn wait = spin_wait(now);
  const std::size_t n = workload.datagrams.size();
  std::vector<Output> out;

  if (stage == Stage::Handler) {
    nasdaq::Handler handler{CoreConfig{}};
    return run_open_loop(config, now, wait, [&](std::uint64_t i) {
      out.clear();
      handler.on_packet(now(), Line::A, workload.datagrams[static_cast<std::size_t>(i) % n], out);
    });
  }

  // Core only: decode once, outside the measurement, then step the core.
  std::vector<PacketIn> packets;
  packets.reserve(n);
  nasdaq::PacketDecoder decoder;
  for (const auto& datagram : workload.datagrams) {
    if (auto packet = decoder.decode(0, Line::A, datagram)) packets.push_back(std::move(*packet));
  }
  Core core{CoreConfig{}};
  return run_open_loop(config, now, wait, [&](std::uint64_t i) {
    out.clear();
    PacketIn& packet = packets[static_cast<std::size_t>(i) % packets.size()];
    packet.t = now();
    core.step(Input{packet}, out);
  });
}

}  // namespace fh::bench
