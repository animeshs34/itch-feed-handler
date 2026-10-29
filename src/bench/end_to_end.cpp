#include "fh/bench/end_to_end.hpp"

#include <unistd.h>

#include <atomic>
#include <memory>
#include <span>
#include <thread>
#include <vector>

#include "fh/bench/open_loop.hpp"
#include "fh/core.hpp"
#include "fh/nasdaq/handler.hpp"
#include "fh/net/clock.hpp"
#include "fh/net/pump.hpp"
#include "fh/net/udp.hpp"

namespace fh::bench {
namespace {

// The bytes 10..17 of a MoldUDP64 packet are the sequence number of its first message.
std::uint64_t first_seq_of(std::span<const std::uint8_t> datagram) {
  std::uint64_t v = 0;
  for (std::size_t i = 10; i < 18 && i < datagram.size(); ++i) v = (v << 8) | datagram[i];
  return v;
}

}  // namespace

double EndToEndResult::achieved_rate() const noexcept {
  return wall <= 0 ? 0.0
                   : static_cast<double>(received) * static_cast<double>(kSecond) /
                         static_cast<double>(wall);
}

std::optional<EndToEndResult> bench_end_to_end(const Workload& workload,
                                               double datagrams_per_second, std::uint64_t count,
                                               std::uint64_t warmup, std::string& error) {
  const std::uint64_t total = warmup + count;
  if (total > workload.datagrams.size() || workload.max_pack != 1) {
    error =
        "the end-to-end benchmark needs one message per datagram and at most as many items as "
        "datagrams";
    return std::nullopt;
  }

  net::Endpoint endpoint;
  endpoint.port = static_cast<std::uint16_t>(34000 + (static_cast<unsigned>(::getpid()) % 2000));
  auto receiver = net::UdpReceiver::open(endpoint, error);
  if (!receiver) return std::nullopt;
  auto sender = net::UdpSender::open(endpoint, error);
  if (!sender) return std::nullopt;

  const net::MonotonicClock clock;
  const Nanos period = static_cast<Nanos>(static_cast<double>(kSecond) / datagrams_per_second);

  // The sender writes each datagram's due time before sending it; the receiver reads it
  // after the datagram arrives, which orders the two.
  const auto due = std::make_unique<std::atomic<Nanos>[]>(static_cast<std::size_t>(total));
  std::atomic<std::uint64_t> sent{0};
  std::atomic<bool> sender_done{false};
  std::atomic<Nanos> sender_done_at{0};

  const ClockFn now = [&clock] { return clock.now(); };
  const WaitFn wait = spin_wait(now);

  EndToEndResult result;
  result.offered_rate = datagrams_per_second;
  Nanos first_due = 0;
  Nanos last_done = 0;

  const Nanos start = clock.now() + 50 * kMillisecond;  // let the receiver get ready
  std::thread sender_thread([&] {
    for (std::uint64_t i = 0; i < total; ++i) {
      const Nanos d = start + static_cast<Nanos>(i) * period;
      due[static_cast<std::size_t>(i)].store(d, std::memory_order_release);
      wait(d);
      if (sender->send(workload.datagrams[static_cast<std::size_t>(i)]))
        sent.fetch_add(1, std::memory_order_release);
    }
    sender_done_at.store(clock.now(), std::memory_order_release);
    sender_done.store(true, std::memory_order_release);
  });

  nasdaq::Handler handler{CoreConfig{}};
  std::vector<Output> out;
  net::DatagramPump pump({&*receiver}, now, 500 * kMicrosecond);
  std::uint64_t received_total = 0;
  pump.run(
      [&](std::size_t, Nanos t, std::span<const std::uint8_t> bytes) {
        out.clear();
        handler.on_packet(t, Line::A, bytes, out);
        const Nanos done = clock.now();
        ++received_total;
        const std::uint64_t index =
            first_seq_of(bytes) - 1;  // one message per datagram, numbered from 1
        if (index < total && index >= warmup) {
          const Nanos d = due[static_cast<std::size_t>(index)].load(std::memory_order_acquire);
          if (result.received == 0) first_due = d;
          ++result.received;
          result.latency.record(done - d);
          last_done = done;
        }
      },
      [&](Nanos t) {
        out.clear();
        handler.on_tick(t, out);
      },
      [&] {
        if (!sender_done.load(std::memory_order_acquire)) return false;
        // Everything sent has arrived, or nothing new has for half a second.
        return received_total >= sent.load(std::memory_order_acquire) ||
               clock.now() - sender_done_at.load(std::memory_order_acquire) > 500 * kMillisecond;
      });
  sender_thread.join();

  result.sent = sent.load() > warmup ? sent.load() - warmup : 0;
  result.wall = last_done - first_due;
  return result;
}

}  // namespace fh::bench
