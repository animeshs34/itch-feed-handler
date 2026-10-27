#include <catch2/catch_test_macros.hpp>

#include "fh/sim/coverage.hpp"
#include "fh/sim/scenario.hpp"

TEST_CASE("an empty table lists every probe with zeros", "[sim][coverage]") {
  const fh::sim::Coverage c;
  REQUIRE(c.scenarios() == 0);
  REQUIRE(c.rows().size() > 15);
  for (const auto& r : c.rows()) {
    REQUIRE(r.scenarios == 0);
    REQUIRE(r.total == 0);
  }
  REQUIRE_FALSE(c.unreached().empty());
  REQUIRE(c.violated().empty());
}

TEST_CASE("adding a verdict counts scenarios and totals separately", "[sim][coverage]") {
  fh::sim::Coverage c;
  fh::sim::Verdict a;
  a.counters.duplicates = 5;
  fh::sim::Verdict b;
  b.counters.duplicates = 2;
  fh::sim::Verdict none;
  c.add(a);
  c.add(b);
  c.add(none);
  REQUIRE(c.scenarios() == 3);
  const auto row = std::find_if(c.rows().begin(), c.rows().end(),
                                [](const auto& r) { return r.name == "identical duplicates"; });
  REQUIRE(row != c.rows().end());
  REQUIRE(row->scenarios == 2);
  REQUIRE(row->total == 7);
}

TEST_CASE("a must-be-zero probe that fires is reported as violated", "[sim][coverage]") {
  fh::sim::Coverage c;
  fh::sim::Verdict v;
  v.alarms[static_cast<std::size_t>(fh::AlarmKind::BookInconsistent)] = 1;
  v.safety_failures.push_back("P9: ...");
  c.add(v);
  const auto bad = c.violated();
  REQUIRE(bad.size() == 2);
}

TEST_CASE("the heavy tier reaches every behaviour the table expects", "[sim][coverage]") {
  fh::sim::Coverage c;
  for (std::uint64_t seed = 1; seed <= 150; ++seed) {
    c.add(fh::sim::run_trace(
        fh::sim::make_trace(seed, fh::sim::scenario_config(fh::sim::Tier::Brutal))));
  }
  for (const auto& name : c.unreached()) UNSCOPED_INFO("never reached: " << name);
  REQUIRE(c.unreached().empty());
  REQUIRE(c.violated().empty());
}

TEST_CASE("the rendered table has a header and one line per probe", "[sim][coverage]") {
  const fh::sim::Coverage c;
  const std::string text = c.render();
  const auto lines = static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
  REQUIRE(lines == c.rows().size() + 1);
  REQUIRE(text.starts_with("probe"));
}
