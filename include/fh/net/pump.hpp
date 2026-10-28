#ifndef FH_NET_PUMP_HPP_
#define FH_NET_PUMP_HPP_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "fh/net/udp.hpp"
#include "fh/time.hpp"

namespace fh::net {

// What the loop did, for a report.
struct PumpStats {
  std::vector<std::uint64_t> datagrams;  // per source
  std::vector<std::uint64_t> bytes;      // per source
  std::uint64_t truncated{0};            // datagrams that did not fit the buffer
  std::uint64_t receive_errors{0};
  std::uint64_t ticks{0};
  std::uint64_t wakeups{0};  // times the loop came back from waiting
  std::size_t largest_datagram{0};
};

// The loop at the edge of the program. It waits for datagrams on several sockets, stamps
// each with the clock the moment it is read, hands it to a callback, and, whenever a
// tick period has passed, hands a tick to another callback so that the handler can notice
// silence. It knows nothing about what the datagrams mean.
//
// The stamp is taken in user space after the read, so it includes the time the datagram
// spent waiting in the kernel buffer. Kernel timestamps (SO_TIMESTAMPNS) would remove that
// and are not used.
class DatagramPump {
 public:
  using ClockFn = std::function<Nanos()>;
  using DatagramFn =
      std::function<void(std::size_t source, Nanos t, std::span<const std::uint8_t>)>;
  using TickFn = std::function<void(Nanos t)>;
  using StopFn = std::function<bool()>;

  // `sources` are not owned and must outlive the pump. `tick_period` must be positive.
  DatagramPump(std::vector<UdpReceiver*> sources, ClockFn clock, Nanos tick_period);

  // Runs until `stop()` returns true. `stop` is asked once per loop, so the loop wakes at
  // least every tick period. Datagrams that arrive in the same wake-up are delivered
  // source by source, each source until it is empty.
  void run(const DatagramFn& on_datagram, const TickFn& on_tick, const StopFn& stop);

  [[nodiscard]] const PumpStats& stats() const noexcept { return stats_; }

 private:
  void drain(std::size_t source, const DatagramFn& on_datagram);

  std::vector<UdpReceiver*> sources_;
  ClockFn clock_;
  Nanos tick_period_;
  std::vector<std::uint8_t> buffer_;
  PumpStats stats_;
};

}  // namespace fh::net

#endif  // FH_NET_PUMP_HPP_
