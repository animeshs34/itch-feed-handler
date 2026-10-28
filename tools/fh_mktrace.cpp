// fh_mktrace: writes the trace of one simulated day to a file, so that it can be replayed
// (fh_replay) or played out on sockets (fh_send).
//
//   fh_mktrace [--seed S] [--tier clean|mild|moderate|heavy|brutal] FILE.fhtr
//
// Exit status: 0 written, 2 bad arguments or the file could not be written.

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "fh/sim/scenario.hpp"
#include "fh/sim/trace_io.hpp"

namespace {

int usage() {
  std::cerr << "usage: fh_mktrace [--seed S] [--tier clean|mild|moderate|heavy|brutal] FILE.fhtr\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  std::uint64_t seed = 1;
  fh::sim::Tier tier = fh::sim::Tier::Clean;
  std::string path;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "--seed") {
      if (i + 1 >= argc) return usage();
      seed = std::strtoull(argv[++i], nullptr, 10);
    } else if (arg == "--tier") {
      if (i + 1 >= argc) return usage();
      const std::string_view name = argv[++i];
      bool found = false;
      for (unsigned t = 0; t < fh::sim::kTierCount; ++t) {
        if (fh::sim::tier_name(static_cast<fh::sim::Tier>(t)) == name) {
          tier = static_cast<fh::sim::Tier>(t);
          found = true;
        }
      }
      if (!found) return usage();
    } else if (!arg.empty() && arg[0] != '-' && path.empty()) {
      path = std::string(arg);
    } else {
      return usage();
    }
  }
  if (path.empty()) return usage();

  const fh::sim::Trace trace = fh::sim::make_trace(seed, fh::sim::scenario_config(tier));
  if (!fh::sim::write_trace_file(path, trace)) {
    std::cerr << "fh_mktrace: cannot write " << path << "\n";
    return 2;
  }
  std::cout << "wrote " << trace.entries.size() << " entries to " << path << "\n";
  return 0;
}
