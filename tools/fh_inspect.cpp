// fh_inspect: reads a BinaryFILE of ITCH 5.0 messages and reports what is in it.
//
//   fh_inspect [--instruments 1,2,3] [--limit N] [--allow-incomplete] FILE|-
//
// FILE of `-` reads standard input, so a compressed file is inspected with
//   gunzip -c S052814-v50.txt.gz | fh_inspect -
//
// It prints the number of messages by type, the busiest instruments, whether the file ends
// properly, and whether it is internally consistent.
//
// Exit status: 0 the file loads and is consistent, 1 it loads but is inconsistent or
// refused, 2 bad arguments or an unreadable file.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "fh/sim/consistency.hpp"
#include "fh/sim/recorded.hpp"

namespace {

int usage() {
  std::cerr << "usage: fh_inspect [--instruments 1,2,3] [--limit N] [--allow-incomplete] FILE|-\n";
  return 2;
}

bool parse_instruments(const std::string& text, std::set<fh::InstrumentId>& out) {
  std::stringstream in(text);
  std::string item;
  while (std::getline(in, item, ',')) {
    char* end = nullptr;
    const unsigned long v = std::strtoul(item.c_str(), &end, 10);
    if (item.empty() || *end != '\0' || v == 0 || v > 65535) return false;
    out.insert(static_cast<fh::InstrumentId>(v));
  }
  return !out.empty();
}

}  // namespace

int main(int argc, char** argv) {
  fh::sim::LoadOptions options;
  std::string path;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "--instruments") {
      if (i + 1 >= argc || !parse_instruments(argv[++i], options.only)) return usage();
    } else if (arg == "--limit") {
      if (i + 1 >= argc) return usage();
      options.limit = std::strtoull(argv[++i], nullptr, 10);
    } else if (arg == "--allow-incomplete") {
      options.allow_incomplete = true;
    } else if (arg == "-" || (!arg.empty() && arg[0] != '-')) {
      if (!path.empty()) return usage();
      path = std::string(arg);
    } else {
      return usage();
    }
  }
  if (path.empty()) return usage();

  std::ifstream file;
  std::istream* in = &std::cin;
  if (path != "-") {
    file.open(path, std::ios::binary);
    if (!file) {
      std::cerr << "fh_inspect: cannot open " << path << "\n";
      return 2;
    }
    in = &file;
  }

  const fh::sim::LoadResult result = fh::sim::load_recorded(*in, options);
  const fh::sim::Census& c = result.census;
  std::printf("status: %s\n", fh::sim::to_string(result.status).c_str());
  if (result.record != 0)
    std::printf("stopped at record %llu\n", static_cast<unsigned long long>(result.record));
  std::printf("messages read: %llu (%llu payload bytes), kept: %llu, book events kept: %llu\n",
              static_cast<unsigned long long>(c.records), static_cast<unsigned long long>(c.bytes),
              static_cast<unsigned long long>(c.kept),
              static_cast<unsigned long long>(c.book_events));
  std::printf("unknown message types: %llu\n", static_cast<unsigned long long>(c.unknown_types));

  std::printf("\nby type:\n");
  for (unsigned t = 0; t < 256; ++t) {
    if (c.by_type[t] == 0) continue;
    std::printf("  %c  %12llu\n", t >= 32 && t < 127 ? static_cast<char>(t) : '?',
                static_cast<unsigned long long>(c.by_type[t]));
  }

  std::vector<std::pair<std::uint64_t, fh::InstrumentId>> busiest;
  for (const auto& [inst, n] : c.by_instrument) busiest.emplace_back(n, inst);
  std::sort(busiest.rbegin(), busiest.rend());
  std::printf("\ninstruments: %zu; the busiest (stock locate, messages):\n", busiest.size());
  for (std::size_t i = 0; i < busiest.size() && i < 10; ++i) {
    std::printf("  %5u  %12llu\n", busiest[i].second,
                static_cast<unsigned long long>(busiest[i].first));
  }

  if (!result.truth.messages.empty()) {
    const fh::sim::ConsistencyReport report = fh::sim::check_consistency(result.truth);
    std::printf("\nconsistency: %s (%llu book events checked, %llu violations)\n",
                report.ok() ? "ok" : "VIOLATIONS",
                static_cast<unsigned long long>(report.events_checked),
                static_cast<unsigned long long>(report.violations));
    for (const fh::sim::Violation& v : report.first) {
      std::printf("  message %llu: kind %d, instrument %u, order %llu\n",
                  static_cast<unsigned long long>(v.seq), static_cast<int>(v.kind), v.inst,
                  static_cast<unsigned long long>(v.order));
    }
    std::printf("input digest: %016llx\n",
                static_cast<unsigned long long>(fh::sim::input_digest(result.truth)));
    return (result.ok() && report.ok()) ? 0 : 1;
  }
  return 1;
}
