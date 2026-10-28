// fh_mkfile: writes a synthetic trading day as a BinaryFILE of ITCH 5.0 messages, in the
// framing Nasdaq's historical files use. It lets the file tools and the recorded-file
// campaign be tried without a real file.
//
//   fh_mkfile [--seed S] [--instruments N] [--events N] [--no-terminator] FILE
//
// Exit status: 0 written, 2 bad arguments or the file could not be written.

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "fh/nasdaq/binary_file.hpp"
#include "fh/sim/day.hpp"
#include "fh/sim/wire.hpp"

namespace {

int usage() {
  std::cerr
      << "usage: fh_mkfile [--seed S] [--instruments N] [--events N] [--no-terminator] FILE\n";
  return 2;
}

bool parse_u64(const char* text, std::uint64_t& out) {
  char* end = nullptr;
  const unsigned long long v = std::strtoull(text, &end, 10);
  if (end == text || *end != '\0') return false;
  out = v;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  std::uint64_t seed = 1;
  fh::sim::DayConfig day;
  bool terminator = true;
  std::string path;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    std::uint64_t n = 0;
    if (arg == "--seed" || arg == "--instruments" || arg == "--events") {
      if (i + 1 >= argc || !parse_u64(argv[++i], n)) return usage();
      if (arg == "--seed") seed = n;
      if (arg == "--instruments") {
        if (n == 0 || n > 60000) return usage();
        day.instruments = static_cast<std::uint16_t>(n);
      }
      if (arg == "--events") day.book_events = static_cast<std::size_t>(n);
    } else if (arg == "--no-terminator") {
      terminator = false;
    } else if (!arg.empty() && arg[0] != '-' && path.empty()) {
      path = std::string(arg);
    } else {
      return usage();
    }
  }
  if (path.empty()) return usage();

  const fh::sim::GroundTruth truth = fh::sim::make_day(seed, day);
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  fh::nasdaq::BinaryFileWriter writer(file);
  for (const fh::sim::GroundMessage& m : truth.messages) {
    if (!writer.write(fh::sim::message_bytes(m))) {
      std::cerr << "fh_mkfile: cannot write " << path << "\n";
      return 2;
    }
  }
  if (terminator && !writer.write_end_of_session()) {
    std::cerr << "fh_mkfile: cannot write " << path << "\n";
    return 2;
  }
  std::cout << "wrote " << truth.messages.size() << " messages to " << path << "\n";
  return 0;
}
