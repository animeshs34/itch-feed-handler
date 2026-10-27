#include <catch2/catch_test_macros.hpp>

#include "fh/sim/campaign.hpp"

TEST_CASE("a mixed campaign uses the five tiers in turn", "[sim][campaign]") {
  fh::sim::CampaignConfig config;
  for (std::uint64_t seed = 0; seed < 10; ++seed) {
    REQUIRE(static_cast<unsigned>(fh::sim::tier_of(config, seed)) == seed % 5);
  }
  config.tier = fh::sim::Tier::Heavy;
  REQUIRE(fh::sim::tier_of(config, 3) == fh::sim::Tier::Heavy);
}

TEST_CASE("a small campaign passes every property", "[sim][campaign]") {
  fh::sim::CampaignConfig config;
  config.scenarios = 100;
  config.metamorphic_every = 10;
  const fh::sim::CampaignReport report = fh::sim::run_campaign(config);
  for (const auto& f : report.failures) {
    for (const auto& m : f.messages) UNSCOPED_INFO("seed " << f.seed << ": " << m);
  }
  REQUIRE(report.passed());
  REQUIRE(report.failing_scenarios == 0);
  REQUIRE(report.coverage.scenarios() == 100);
  REQUIRE(report.metamorphic_runs == 10);
  for (const auto& t : report.tiers) REQUIRE(t.scenarios == 20);
}

TEST_CASE("the same campaign gives the same digest, and a different one a different digest",
          "[sim][campaign]") {
  fh::sim::CampaignConfig config;
  config.scenarios = 30;
  config.metamorphic_every = 0;
  const auto a = fh::sim::run_campaign(config);
  const auto b = fh::sim::run_campaign(config);
  REQUIRE(a.digest == b.digest);
  config.first_seed = 31;
  REQUIRE(fh::sim::run_campaign(config).digest != a.digest);
}

TEST_CASE("a campaign can be split in two and the parts cover the whole", "[sim][campaign]") {
  fh::sim::CampaignConfig whole;
  whole.scenarios = 40;
  whole.metamorphic_every = 0;
  const auto all = fh::sim::run_campaign(whole);

  fh::sim::CampaignConfig first = whole, second = whole;
  first.scenarios = 25;
  second.first_seed = 26;
  second.scenarios = 15;
  const auto a = fh::sim::run_campaign(first);
  const auto b = fh::sim::run_campaign(second);
  for (unsigned t = 0; t < fh::sim::kTierCount; ++t) {
    REQUIRE(a.tiers[t].scenarios + b.tiers[t].scenarios == all.tiers[t].scenarios);
  }
  REQUIRE(a.coverage.rows()[0].total + b.coverage.rows()[0].total == all.coverage.rows()[0].total);
}

TEST_CASE("the report says PASS, lists the tiers and ends with the digest", "[sim][campaign]") {
  fh::sim::CampaignConfig config;
  config.scenarios = 10;
  const std::string text = fh::sim::run_campaign(config).render();
  REQUIRE(text.starts_with("campaign: seeds 1..10 (10 scenarios)"));
  REQUIRE(text.find("brutal") != std::string::npos);
  REQUIRE(text.find("digest: ") != std::string::npos);
  REQUIRE(text.find("result: PASS") != std::string::npos);
}

#include <sstream>

#include "fh/nasdaq/binary_file.hpp"
#include "fh/sim/day.hpp"
#include "fh/sim/recorded.hpp"
#include "fh/sim/wire.hpp"

TEST_CASE("a campaign on a recorded day passes and reports its source", "[sim][campaign]") {
  // A synthetic day, written as a file and read back, stands in for a recorded one.
  const fh::sim::GroundTruth day = fh::sim::make_day(11, fh::sim::DayConfig{});
  std::stringstream file;
  fh::nasdaq::BinaryFileWriter writer(file);
  for (const auto& m : day.messages) REQUIRE(writer.write(fh::sim::message_bytes(m)));
  REQUIRE(writer.write_end_of_session());
  const auto loaded = fh::sim::load_recorded(file, fh::sim::LoadOptions{});
  REQUIRE(loaded.ok());

  fh::sim::CampaignConfig config;
  config.scenarios = 50;
  config.recorded = &loaded.truth;
  const auto report = fh::sim::run_campaign(config);
  for (const auto& f : report.failures) {
    for (const auto& m : f.messages) UNSCOPED_INFO("seed " << f.seed << ": " << m);
  }
  REQUIRE(report.passed());
  const std::string expected =
      "source: recorded day, " + std::to_string(day.messages.size()) + " messages";
  REQUIRE(report.render().find(expected) != std::string::npos);
}

TEST_CASE("the recorded day goes through the handler as its exact bytes", "[sim][campaign]") {
  // A day with a message type the synthetic generator never writes, kept as recorded bytes,
  // must reach the handler unchanged and count as an unknown type, not a refusal.
  fh::sim::GroundTruth day = fh::sim::make_day(12, fh::sim::DayConfig{});
  fh::sim::GroundMessage unknown;
  unknown.seq = day.messages.size() + 1;
  unknown.filler = '#';
  unknown.locate = 0;
  unknown.raw = {'#', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7};
  day.messages.push_back(unknown);

  fh::sim::CampaignConfig config;
  config.scenarios = 10;
  config.tier = fh::sim::Tier::Clean;
  config.recorded = &day;
  REQUIRE(fh::sim::run_campaign(config).passed());
}
