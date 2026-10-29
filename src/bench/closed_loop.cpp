#include "fh/bench/closed_loop.hpp"

#include <chrono>
#include <vector>

#include "fh/core.hpp"
#include "fh/nasdaq/handler.hpp"
#include "fh/nasdaq/moldudp64.hpp"
#include "fh/net/clock.hpp"

namespace fh::bench {
namespace {

double seconds_between(std::chrono::steady_clock::time_point a,
                       std::chrono::steady_clock::time_point b) {
  return std::chrono::duration<double>(b - a).count();
}

}  // namespace

ClosedLoopResult bench_handler_closed_loop(const Workload& workload, unsigned repeats) {
  ClosedLoopResult result;
  std::vector<Output> out;
  const net::MonotonicClock clock;

  // Timed pass: one clock reading before and after every call.
  for (unsigned r = 0; r < repeats; ++r) {
    nasdaq::Handler handler{CoreConfig{}};
    Nanos t = 0;
    for (const auto& datagram : workload.datagrams) {
      out.clear();
      t += 100 *
           kMicrosecond;  // simulated receive times; the handler's own time is not what is measured
      const Nanos begin = clock.now();
      handler.on_packet(t, Line::A, datagram, out);
      result.service.record(clock.now() - begin);
    }
  }

  // Untimed pass: wall time over the whole run, with no per-datagram clock readings.
  nasdaq::Handler handler{CoreConfig{}};
  Nanos t = 0;
  std::uint64_t outputs = 0;
  const auto begin = std::chrono::steady_clock::now();
  for (const auto& datagram : workload.datagrams) {
    out.clear();
    t += 100 * kMicrosecond;
    handler.on_packet(t, Line::A, datagram, out);
    outputs += out.size();
  }
  const double seconds = seconds_between(begin, std::chrono::steady_clock::now());
  result.messages = workload.messages;
  result.outputs = outputs;
  result.messages_per_second = static_cast<double>(workload.messages) / seconds;
  result.datagrams_per_second = static_cast<double>(workload.datagrams.size()) / seconds;
  return result;
}

ClosedLoopResult bench_core_closed_loop(const Workload& workload, unsigned repeats) {
  // Decode once, outside the measurement.
  std::vector<PacketIn> packets;
  packets.reserve(workload.datagrams.size());
  nasdaq::PacketDecoder decoder;
  Nanos t = 0;
  for (const auto& datagram : workload.datagrams) {
    t += 100 * kMicrosecond;
    if (auto packet = decoder.decode(t, Line::A, datagram)) packets.push_back(std::move(*packet));
  }

  ClosedLoopResult result;
  std::vector<Output> out;
  const net::MonotonicClock clock;
  for (unsigned r = 0; r < repeats; ++r) {
    Core core{CoreConfig{}};
    for (const PacketIn& packet : packets) {
      out.clear();
      const Input input{packet};
      const Nanos begin = clock.now();
      core.step(input, out);
      result.service.record(clock.now() - begin);
    }
  }

  Core core{CoreConfig{}};
  std::uint64_t outputs = 0;
  const auto begin = std::chrono::steady_clock::now();
  for (const PacketIn& packet : packets) {
    out.clear();
    core.step(Input{packet}, out);
    outputs += out.size();
  }
  const double seconds = seconds_between(begin, std::chrono::steady_clock::now());
  result.messages = workload.messages;
  result.outputs = outputs;
  result.messages_per_second = static_cast<double>(workload.messages) / seconds;
  result.datagrams_per_second = static_cast<double>(packets.size()) / seconds;
  return result;
}

}  // namespace fh::bench
