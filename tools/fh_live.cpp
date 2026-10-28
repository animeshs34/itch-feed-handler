// fh_live: listens on the three multicast groups, runs the Nasdaq handler on what arrives,
// and reports. It can save everything it was shown as a trace.
//
//   fh_live [--port P] [--group ADDR] [--interface ADDR] [--seconds S]
//           [--record FILE.fhtr] [--truth TRACE.fhtr] [--scale N]
//
// The groups are on ports P, P+1 and P+2 (line A, line B, snapshots). It stops after S
// seconds (default 10). With --truth, the true feed and the core settings of that trace are
// attached to the recording, and the recording is checked against the true feed; --scale
// must then match the sender's.
//
// Exit status: 0 all good (or nothing to check), 1 a property failed, 2 bad arguments or the
// groups could not be joined.

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "fh/live/live.hpp"
#include "fh/net/clock.hpp"
#include "fh/sim/checker.hpp"
#include "fh/sim/trace_io.hpp"

namespace {

int usage() {
  std::cerr << "usage: fh_live [--port P] [--group ADDR] [--interface ADDR] [--seconds S]\n"
               "               [--record FILE.fhtr] [--truth TRACE.fhtr] [--scale N]\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  std::uint16_t port = 31000;
  std::string group = "239.255.42.1";
  std::string interface_address = "127.0.0.1";
  double seconds = 10;
  std::string record_path;
  std::string truth_path;
  std::int64_t scale = 1;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const auto value = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
    const char* v = nullptr;
    if (arg == "--port") {
      if ((v = value()) == nullptr) return usage();
      port = static_cast<std::uint16_t>(std::strtoul(v, nullptr, 10));
    } else if (arg == "--group") {
      if ((v = value()) == nullptr) return usage();
      group = v;
    } else if (arg == "--interface") {
      if ((v = value()) == nullptr) return usage();
      interface_address = v;
    } else if (arg == "--seconds") {
      if ((v = value()) == nullptr) return usage();
      seconds = std::strtod(v, nullptr);
      if (seconds <= 0) return usage();
    } else if (arg == "--record") {
      if ((v = value()) == nullptr) return usage();
      record_path = v;
    } else if (arg == "--truth") {
      if ((v = value()) == nullptr) return usage();
      truth_path = v;
    } else if (arg == "--scale") {
      if ((v = value()) == nullptr) return usage();
      scale = std::strtoll(v, nullptr, 10);
      if (scale < 1) return usage();
    } else {
      return usage();
    }
  }
  if (port == 0 || port > 65533) return usage();

  std::optional<fh::sim::Trace> truth;
  if (!truth_path.empty()) {
    truth = fh::sim::read_trace_file(truth_path);
    if (!truth) {
      std::cerr << "fh_live: cannot read a trace from " << truth_path << "\n";
      return 2;
    }
  }

  fh::live::LiveConfig config;
  config.groups = fh::live::groups_on(port, group, interface_address);
  if (truth) {
    config.core = fh::live::scale_core(truth->core, scale);
    config.tick_period = 200 * fh::kMicrosecond * scale;
  }

  const fh::net::MonotonicClock deadline_clock;
  const fh::Nanos limit = static_cast<fh::Nanos>(seconds * static_cast<double>(fh::kSecond));
  std::string error;
  std::cerr << "fh_live: listening on " << group << " ports " << port << ".." << port + 2 << " for "
            << seconds << " s\n";
  auto result = fh::live::run_live(config, [&] { return deadline_clock.now() >= limit; }, error);
  if (!result) {
    std::cerr << "fh_live: " << error << "\n";
    return 2;
  }

  const auto& s = result->stats;
  std::cout << "datagrams: line A " << s.datagrams[0] << ", line B " << s.datagrams[1]
            << ", snapshots " << s.datagrams[2] << "; truncated " << s.truncated
            << ", receive errors " << s.receive_errors << ", ticks " << s.ticks << "\n";
  std::cout << "outputs published: " << result->outputs.size() << "\n";
  std::cout << "final books: " << (result->final_state ? "every instrument live" : "not all live")
            << "\n";

  int status = 0;
  if (truth) {
    result->trace.truth = truth->truth;
    const fh::sim::Verdict verdict = fh::sim::run_trace(result->trace);
    for (const std::string& f : verdict.safety_failures) std::cout << "FAIL (safety) " << f << "\n";
    for (const std::string& f : verdict.liveness_failures)
      std::cout << "FAIL (liveness) " << f << "\n";
    std::cout << (verdict.ok() ? "result: PASS\n" : "result: FAIL\n");
    if (!verdict.ok()) status = 1;
  }
  if (!record_path.empty()) {
    if (!fh::sim::write_trace_file(record_path, result->trace)) {
      std::cerr << "fh_live: cannot write " << record_path << "\n";
      return 2;
    }
    std::cout << "recorded " << result->trace.entries.size() << " entries to " << record_path
              << "\n";
  }
  return status;
}
