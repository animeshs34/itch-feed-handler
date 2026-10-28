// fh_campaign: runs a campaign of simulated days through the Nasdaq handler and checks it.
//
//   fh_campaign [--scenarios N] [--first-seed S] [--tier clean|mild|moderate|heavy|brutal|mixed]
//               [--metamorphic-every K] [--out DIR] [--no-shrink] [--require-coverage] [--quiet]
//               [--file FILE|- [--instruments 1,2,3] [--limit N] [--allow-incomplete]]
//
// With --file, every scenario replays the day recorded in that BinaryFILE of ITCH 5.0
// messages (read from standard input for `-`) instead of a synthetic day; only the faults
// change from seed to seed. The file must be internally consistent.
//
// Exit status: 0 all scenarios passed, 1 some scenario failed, 2 bad arguments,
// 3 everything passed but a probe that should have been reached never was
// (only with --require-coverage), 4 the recorded file could not be used.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

#include "fh/sim/campaign.hpp"
#include "fh/sim/consistency.hpp"
#include "fh/sim/recorded.hpp"

namespace {

void usage() {
  std::cerr << "usage: fh_campaign [--scenarios N] [--first-seed S] "
               "[--tier clean|mild|moderate|heavy|brutal|mixed]\n"
               "                   [--metamorphic-every K] [--out DIR] [--no-shrink] "
               "[--require-coverage] [--quiet]\n"
               "                   [--file FILE|- [--instruments 1,2,3] [--limit N] "
               "[--allow-incomplete]]\n";
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
  fh::sim::CampaignConfig config;
  bool require_coverage = false;
  bool quiet = false;
  std::string file_path;
  fh::sim::LoadOptions load_options;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const auto value = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
    std::uint64_t n = 0;
    if (arg == "--scenarios") {
      const char* v = value();
      if (v == nullptr || !parse_u64(v, n) || n == 0) return usage(), 2;
      config.scenarios = n;
    } else if (arg == "--first-seed") {
      const char* v = value();
      if (v == nullptr || !parse_u64(v, n)) return usage(), 2;
      config.first_seed = n;
    } else if (arg == "--metamorphic-every") {
      const char* v = value();
      if (v == nullptr || !parse_u64(v, n)) return usage(), 2;
      config.metamorphic_every = static_cast<unsigned>(n);
    } else if (arg == "--tier") {
      const char* v = value();
      if (v == nullptr) return usage(), 2;
      const std::string_view name = v;
      if (name != "mixed") {
        bool found = false;
        for (unsigned t = 0; t < fh::sim::kTierCount; ++t) {
          if (fh::sim::tier_name(static_cast<fh::sim::Tier>(t)) == name) {
            config.tier = static_cast<fh::sim::Tier>(t);
            found = true;
          }
        }
        if (!found) return usage(), 2;
      }
    } else if (arg == "--out") {
      const char* v = value();
      if (v == nullptr) return usage(), 2;
      config.out_dir = v;
    } else if (arg == "--no-shrink") {
      config.shrink = false;
    } else if (arg == "--require-coverage") {
      require_coverage = true;
    } else if (arg == "--quiet") {
      quiet = true;
    } else if (arg == "--file") {
      const char* v = value();
      if (v == nullptr) return usage(), 2;
      file_path = v;
    } else if (arg == "--limit") {
      const char* v = value();
      if (v == nullptr || !parse_u64(v, n)) return usage(), 2;
      load_options.limit = n;
    } else if (arg == "--allow-incomplete") {
      load_options.allow_incomplete = true;
    } else if (arg == "--instruments") {
      const char* v = value();
      if (v == nullptr) return usage(), 2;
      std::stringstream list(v);
      std::string item;
      while (std::getline(list, item, ',')) {
        char* end = nullptr;
        const unsigned long id = std::strtoul(item.c_str(), &end, 10);
        if (item.empty() || *end != '\0' || id == 0 || id > 65535) return usage(), 2;
        load_options.only.insert(static_cast<fh::InstrumentId>(id));
      }
    } else {
      return usage(), 2;
    }
  }

  fh::sim::LoadResult recorded;
  if (!file_path.empty()) {
    std::ifstream file;
    std::istream* in = &std::cin;
    if (file_path != "-") {
      file.open(file_path, std::ios::binary);
      if (!file) {
        std::cerr << "fh_campaign: cannot open " << file_path << "\n";
        return 4;
      }
      in = &file;
    }
    recorded = fh::sim::load_recorded(*in, load_options);
    if (!recorded.ok()) {
      std::cerr << "fh_campaign: the file cannot be used: " << fh::sim::to_string(recorded.status)
                << "\n";
      return 4;
    }
    const fh::sim::ConsistencyReport consistency = fh::sim::check_consistency(recorded.truth);
    if (!consistency.ok()) {
      std::cerr << "fh_campaign: the file is not internally consistent (" << consistency.violations
                << " violations, the first at message " << consistency.first.front().seq << ")\n";
      return 4;
    }
    config.recorded = &recorded.truth;
  }

  const fh::sim::CampaignReport report =
      fh::sim::run_campaign(config, quiet ? nullptr : &std::cerr);
  std::cout << report.render();
  if (config.recorded != nullptr) {
    std::printf("input digest: %016llx\n",
                static_cast<unsigned long long>(fh::sim::input_digest(recorded.truth)));
  }
  if (!report.passed()) return 1;
  if (require_coverage && !report.coverage.unreached().empty()) return 3;
  return 0;
}
