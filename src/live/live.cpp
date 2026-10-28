#include "fh/live/live.hpp"

#include <chrono>
#include <thread>

#include "fh/nasdaq/handler.hpp"
#include "fh/net/clock.hpp"

namespace fh::live {

Groups groups_on(std::uint16_t first_port, const std::string& group_address,
                 const std::string& interface_address) {
  Groups g;
  g.line_a = net::Endpoint{group_address, first_port, interface_address};
  g.line_b =
      net::Endpoint{group_address, static_cast<std::uint16_t>(first_port + 1), interface_address};
  g.snapshots =
      net::Endpoint{group_address, static_cast<std::uint16_t>(first_port + 2), interface_address};
  return g;
}

sim::CoreParams scale_core(const sim::CoreParams& params, std::int64_t factor) {
  sim::CoreParams out = params;
  out.skew_window *= factor;
  out.silent_window *= factor;
  return out;
}

std::optional<LiveResult> run_live(const LiveConfig& config, const std::function<bool()>& stop,
                                   std::string& error) {
  auto line_a = net::UdpReceiver::open(config.groups.line_a, error);
  if (!line_a) return std::nullopt;
  auto line_b = net::UdpReceiver::open(config.groups.line_b, error);
  if (!line_b) return std::nullopt;
  auto snapshots = net::UdpReceiver::open(config.groups.snapshots, error);
  if (!snapshots) return std::nullopt;

  // Source numbers are the order given to the pump: line A, line B, the snapshot stream.
  constexpr std::size_t kLineA = 0;
  constexpr std::size_t kSnapshots = 2;

  LiveResult result;
  result.trace.core = config.core;
  nasdaq::Handler handler(sim::to_core_config(config.core));
  std::vector<Output> out;
  const auto publish = [&] {
    if (config.collect_outputs) result.outputs.insert(result.outputs.end(), out.begin(), out.end());
    out.clear();
  };

  const net::MonotonicClock clock;
  net::DatagramPump pump(
      {&*line_a, &*line_b, &*snapshots}, [&clock] { return clock.now(); }, config.tick_period);
  pump.run(
      [&](std::size_t source, Nanos t, std::span<const std::uint8_t> bytes) {
        sim::Entry entry;
        entry.t = t;
        entry.bytes.assign(bytes.begin(), bytes.end());
        if (source == kSnapshots) {
          entry.kind = sim::EntryKind::Snapshot;
          handler.on_snapshot_packet(t, bytes, out);
        } else {
          entry.kind = sim::EntryKind::Main;
          entry.line = source == kLineA ? Line::A : Line::B;
          handler.on_packet(t, entry.line, bytes, out);
        }
        result.trace.entries.push_back(std::move(entry));
        publish();
      },
      [&](Nanos t) {
        sim::Entry entry;
        entry.t = t;
        entry.kind = sim::EntryKind::Tick;
        result.trace.entries.push_back(std::move(entry));
        handler.on_tick(t, out);
        publish();
      },
      stop);

  result.stats = pump.stats();
  result.final_state = handler.core().export_state();
  return result;
}

std::optional<SendStats> send_trace(const sim::Trace& trace, const SendConfig& config,
                                    std::string& error) {
  auto line_a = net::UdpSender::open(config.groups.line_a, error);
  if (!line_a) return std::nullopt;
  auto line_b = net::UdpSender::open(config.groups.line_b, error);
  if (!line_b) return std::nullopt;
  auto snapshots = net::UdpSender::open(config.groups.snapshots, error);
  if (!snapshots) return std::nullopt;

  SendStats stats;
  const auto start = std::chrono::steady_clock::now();
  for (const sim::Entry& entry : trace.entries) {
    if (entry.kind == sim::EntryKind::Tick) continue;
    const auto due = start + std::chrono::nanoseconds(entry.t * config.scale);
    std::this_thread::sleep_until(due);
    const auto lateness =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - due)
            .count();
    stats.worst_lateness = std::max<Nanos>(stats.worst_lateness, lateness);

    net::UdpSender& sender = entry.kind == sim::EntryKind::Snapshot
                                 ? *snapshots
                                 : (entry.line == Line::A ? *line_a : *line_b);
    if (sender.send(entry.bytes)) {
      ++stats.sent;
    } else {
      ++stats.failed;
    }
  }
  return stats;
}

}  // namespace fh::live
