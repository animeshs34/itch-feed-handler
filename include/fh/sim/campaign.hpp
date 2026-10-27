#ifndef FH_SIM_CAMPAIGN_HPP_
#define FH_SIM_CAMPAIGN_HPP_

#include <array>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "fh/sim/coverage.hpp"
#include "fh/sim/faults.hpp"
#include "fh/sim/ground_truth.hpp"

namespace fh::sim {

// A campaign is a range of seeds. Scenario `s` is `make_trace(s, scenario_config(tier))`,
// so a campaign is fully described by its numbers and can be repeated exactly.
struct CampaignConfig {
  std::uint64_t first_seed{1};
  std::uint64_t scenarios{100};
  // One tier for every scenario, or (nullopt) all five in turn: seed % 5 picks the tier.
  std::optional<Tier> tier;
  // Every Nth scenario also runs the two-run properties P5 and P7. 0 = never.
  unsigned metamorphic_every{20};
  bool shrink{true};
  std::string out_dir;  // where failing traces are saved; empty = nowhere
  // If set, every scenario uses this (recorded) ground truth instead of a synthetic day;
  // only the faults differ from one seed to the next. Not owned; must outlive the call.
  const GroundTruth* recorded{nullptr};
};

struct TierTotals {
  std::uint64_t scenarios{0};
  std::uint64_t failures{0};
};

struct FailureRecord {
  std::uint64_t seed{0};
  Tier tier{Tier::Clean};
  std::vector<std::string> messages;
  std::string saved_to;           // the saved trace, if out_dir was given
  std::size_t entries_before{0};  // sizes before and after shrinking (equal if not shrunk)
  std::size_t entries_after{0};
};

struct CampaignReport {
  CampaignConfig config;
  std::array<TierTotals, kTierCount> tiers{};
  std::uint64_t metamorphic_runs{0};
  Coverage coverage;
  std::vector<FailureRecord> failures;  // at most kMaxRecordedFailures are kept
  std::uint64_t failing_scenarios{0};
  // A fingerprint of every scenario's published outputs and final books. Two runs of
  // the same campaign, on any compiler, must print the same number.
  std::uint64_t digest{0};

  [[nodiscard]] bool passed() const noexcept { return failing_scenarios == 0; }
  [[nodiscard]] std::string render() const;
};

inline constexpr std::size_t kMaxRecordedFailures = 20;

// The tier a scenario uses under `config`.
[[nodiscard]] Tier tier_of(const CampaignConfig& config, std::uint64_t seed);

// Runs the whole campaign. If `progress` is given, a line is written to it about every
// tenth of the way.
[[nodiscard]] CampaignReport run_campaign(const CampaignConfig& config,
                                          std::ostream* progress = nullptr);

}  // namespace fh::sim

#endif  // FH_SIM_CAMPAIGN_HPP_
