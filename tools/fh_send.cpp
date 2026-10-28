// fh_send: plays a saved trace out on real multicast sockets, so that a separate fh_live
// (or any receiver) can be tried against it.
//
//   fh_send TRACE.fhtr [--port P] [--group ADDR] [--interface ADDR] [--scale N]
//
// The three groups are on ports P, P+1 and P+2 (line A, line B, snapshots). The trace's
// ticks are not sent. An entry stamped t is sent t * N after the start.
//
// Exit status: 0 sent, 1 some datagrams were refused by the kernel, 2 bad arguments or the
// groups could not be opened.

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "fh/live/live.hpp"
#include "fh/sim/trace_io.hpp"

namespace {

int usage() {
  std::cerr
      << "usage: fh_send TRACE.fhtr [--port P] [--group ADDR] [--interface ADDR] [--scale N]\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  std::string path;
  std::uint16_t port = 31000;
  std::string group = "239.255.42.1";
  std::string interface_address = "127.0.0.1";
  std::int64_t scale = 1;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const auto value = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
    if (arg == "--port") {
      const char* v = value();
      if (v == nullptr) return usage();
      port = static_cast<std::uint16_t>(std::strtoul(v, nullptr, 10));
    } else if (arg == "--group") {
      const char* v = value();
      if (v == nullptr) return usage();
      group = v;
    } else if (arg == "--interface") {
      const char* v = value();
      if (v == nullptr) return usage();
      interface_address = v;
    } else if (arg == "--scale") {
      const char* v = value();
      if (v == nullptr) return usage();
      scale = std::strtoll(v, nullptr, 10);
      if (scale < 1) return usage();
    } else if (!arg.empty() && arg[0] != '-' && path.empty()) {
      path = std::string(arg);
    } else {
      return usage();
    }
  }
  if (path.empty() || port == 0 || port > 65533) return usage();

  const auto trace = fh::sim::read_trace_file(path);
  if (!trace) {
    std::cerr << "fh_send: cannot read a trace from " << path << "\n";
    return 2;
  }

  fh::live::SendConfig config;
  config.groups = fh::live::groups_on(port, group, interface_address);
  config.scale = scale;
  std::string error;
  const auto stats = fh::live::send_trace(*trace, config, error);
  if (!stats) {
    std::cerr << "fh_send: " << error << "\n";
    return 2;
  }
  std::cout << "sent " << stats->sent << " datagrams, " << stats->failed
            << " refused, worst lateness " << stats->worst_lateness / fh::kMicrosecond << " us\n";
  return stats->failed == 0 ? 0 : 1;
}
