#include "fh/sim/campaign.hpp"

#include <algorithm>
#include <cstdio>
#include <ostream>

#include "fh/sim/checker.hpp"
#include "fh/sim/metamorphic.hpp"
#include "fh/sim/scenario.hpp"
#include "fh/sim/shrink.hpp"
#include "fh/sim/trace_io.hpp"

namespace fh::sim {
namespace {

// Folds a 64-bit value into a running FNV-1a hash.
std::uint64_t fold(std::uint64_t hash, std::uint64_t value) {
  for (int i = 0; i < 8; ++i) {
    hash ^= (value >> (8 * i)) & 0xFF;
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string file_name(const FailureRecord& f, const char* suffix) {
  return "fail-" + std::string(tier_name(f.tier)) + "-" + std::to_string(f.seed) + suffix + ".fhtr";
}

}  // namespace

Tier tier_of(const CampaignConfig& config, std::uint64_t seed) {
  if (config.tier) return *config.tier;
  return static_cast<Tier>(seed % kTierCount);
}

CampaignReport run_campaign(const CampaignConfig& config, std::ostream* progress) {
  CampaignReport report;
  report.config = config;
  report.digest = 14695981039346656037ULL;

  const std::uint64_t step = std::max<std::uint64_t>(1, config.scenarios / 10);
  for (std::uint64_t i = 0; i < config.scenarios; ++i) {
    const std::uint64_t seed = config.first_seed + i;
    const Tier tier = tier_of(config, seed);
    const Trace trace = config.recorded != nullptr
                            ? make_trace_for(*config.recorded, seed, scenario_config(tier))
                            : make_trace(seed, scenario_config(tier));
    const Verdict verdict = run_trace(trace);

    report.coverage.add(verdict);
    ++report.tiers[static_cast<std::size_t>(tier)].scenarios;
    report.digest =
        fold(fold(fold(report.digest, seed), verdict.output_digest), verdict.final_checksum);

    FailureRecord failure;
    failure.seed = seed;
    failure.tier = tier;
    for (const std::string& m : verdict.safety_failures) failure.messages.push_back(m);
    for (const std::string& m : verdict.liveness_failures) failure.messages.push_back(m);

    if (config.metamorphic_every != 0 && i % config.metamorphic_every == 0) {
      ++report.metamorphic_runs;
      if (const auto m = check_determinism(trace)) failure.messages.push_back(*m);
      if (const auto m = check_duplicate_invariance(trace, seed)) failure.messages.push_back(*m);
    }

    if (!failure.messages.empty()) {
      ++report.failing_scenarios;
      ++report.tiers[static_cast<std::size_t>(tier)].failures;
      failure.entries_before = trace.entries.size();
      failure.entries_after = trace.entries.size();
      if (report.failures.size() < kMaxRecordedFailures) {
        if (!config.out_dir.empty()) {
          failure.saved_to = config.out_dir + "/" + file_name(failure, "");
          (void)write_trace_file(failure.saved_to, trace);
          if (config.shrink && !verdict.safe()) {
            const Trace small = shrink_safety_failure(trace);
            failure.entries_after = small.entries.size();
            (void)write_trace_file(config.out_dir + "/" + file_name(failure, "-shrunk"), small);
          }
        }
        report.failures.push_back(std::move(failure));
      }
    }

    if (progress != nullptr && (i + 1) % step == 0) {
      *progress << (i + 1) << " / " << config.scenarios << " scenarios, "
                << report.failing_scenarios << " failing\n";
    }
  }
  return report;
}

std::string CampaignReport::render() const {
  std::string out;
  char line[256];
  const auto put = [&out, &line](const char* fmt, auto... args) {
    std::snprintf(line, sizeof line, fmt, args...);
    out += line;
  };

  put("campaign: seeds %llu..%llu (%llu scenarios), tier: %s\n",
      static_cast<unsigned long long>(config.first_seed),
      static_cast<unsigned long long>(config.first_seed + config.scenarios - 1),
      static_cast<unsigned long long>(config.scenarios),
      config.tier ? std::string(tier_name(*config.tier)).c_str() : "mixed (seed % 5)");
  if (config.recorded != nullptr) {
    put("source: recorded day, %zu messages (%zu book events), faults vary by seed\n",
        config.recorded->messages.size(), config.recorded->events().size());
  } else {
    out += "source: synthetic days, one per seed\n";
  }
  put("\n%-10s %12s %10s\n", "tier", "scenarios", "failing");
  for (unsigned t = 0; t < kTierCount; ++t) {
    put("%-10s %12llu %10llu\n", std::string(tier_name(static_cast<Tier>(t))).c_str(),
        static_cast<unsigned long long>(tiers[t].scenarios),
        static_cast<unsigned long long>(tiers[t].failures));
  }
  out += "\nproperties checked on every scenario: P1 P2 P3 P4 P9\n";
  put("two-run properties P5 P7 checked on %llu scenarios\n",
      static_cast<unsigned long long>(metamorphic_runs));
  out += "\n" + coverage.render();
  put("\nunreached probes: %zu\n", coverage.unreached().size());
  for (const std::string& name : coverage.unreached()) put("  never reached: %s\n", name.c_str());

  put("\nfailing scenarios: %llu\n", static_cast<unsigned long long>(failing_scenarios));
  for (const FailureRecord& f : failures) {
    put("  seed %llu tier %s (entries %zu -> %zu)%s%s\n", static_cast<unsigned long long>(f.seed),
        std::string(tier_name(f.tier)).c_str(), f.entries_before, f.entries_after,
        f.saved_to.empty() ? "" : " saved ", f.saved_to.c_str());
    for (const std::string& m : f.messages) put("    %s\n", m.c_str());
  }
  if (failing_scenarios > failures.size()) {
    put("  ... and %llu more not listed\n",
        static_cast<unsigned long long>(failing_scenarios - failures.size()));
  }
  put("\ndigest: %016llx\n", static_cast<unsigned long long>(digest));
  put("result: %s\n", passed() ? "PASS" : "FAIL");
  return out;
}

}  // namespace fh::sim
