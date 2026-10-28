#include "fh/net/pump.hpp"

#include <poll.h>

#include <algorithm>
#include <cassert>
#include <ctime>

namespace fh::net {
namespace {

// The largest UDP payload over IPv4 is 65,507 bytes.
constexpr std::size_t kBufferBytes = 65536;

}  // namespace

DatagramPump::DatagramPump(std::vector<UdpReceiver*> sources, ClockFn clock, Nanos tick_period)
    : sources_(std::move(sources)),
      clock_(std::move(clock)),
      tick_period_(tick_period),
      buffer_(kBufferBytes) {
  assert(tick_period_ > 0);
  stats_.datagrams.assign(sources_.size(), 0);
  stats_.bytes.assign(sources_.size(), 0);
}

void DatagramPump::drain(std::size_t source, const DatagramFn& on_datagram) {
  for (;;) {
    std::size_t size = 0;
    switch (sources_[source]->receive(buffer_, size)) {
      case ReceiveStatus::Datagram:
        ++stats_.datagrams[source];
        stats_.bytes[source] += size;
        stats_.largest_datagram = std::max(stats_.largest_datagram, size);
        on_datagram(source, clock_(), std::span<const std::uint8_t>(buffer_.data(), size));
        break;
      case ReceiveStatus::Empty:
        return;
      case ReceiveStatus::Truncated:
        ++stats_.truncated;
        break;
      case ReceiveStatus::Error:
        ++stats_.receive_errors;
        return;
    }
  }
}

void DatagramPump::run(const DatagramFn& on_datagram, const TickFn& on_tick, const StopFn& stop) {
  std::vector<pollfd> fds(sources_.size());
  for (std::size_t i = 0; i < sources_.size(); ++i) fds[i] = pollfd{sources_[i]->fd(), POLLIN, 0};

  Nanos next_tick = clock_() + tick_period_;
  while (!stop()) {
    // Wait for data, but not past the next tick.
    const Nanos wait = std::max<Nanos>(0, next_tick - clock_());
    timespec timeout{};
    timeout.tv_sec = static_cast<time_t>(wait / kSecond);
    timeout.tv_nsec = static_cast<long>(wait % kSecond);
    (void)::ppoll(fds.data(), fds.size(), &timeout, nullptr);
    ++stats_.wakeups;

    for (std::size_t i = 0; i < sources_.size(); ++i) drain(i, on_datagram);

    const Nanos now = clock_();
    if (now >= next_tick) {
      on_tick(now);
      ++stats_.ticks;
      // A long pause must not cause a burst of ticks to catch up.
      next_tick = now + tick_period_;
    }
  }
}

}  // namespace fh::net
