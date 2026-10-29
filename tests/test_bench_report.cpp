#include <sched.h>

#include <catch2/catch_test_macros.hpp>

#include "fh/bench/environment.hpp"
#include "fh/bench/report.hpp"

using namespace fh::bench;

TEST_CASE("durations get a unit that keeps the digits useful", "[bench][report]") {
  REQUIRE(format_ns(fh::Nanos{0}) == "0 ns");
  REQUIRE(format_ns(fh::Nanos{850}) == "850 ns");
  REQUIRE(format_ns(fh::Nanos{1234}) == "1.23 us");
  REQUIRE(format_ns(fh::Nanos{12345}) == "12.3 us");
  REQUIRE(format_ns(fh::Nanos{999999}) == "1000.0 us");
  REQUIRE(format_ns(fh::Nanos{4560000}) == "4.56 ms");
  REQUIRE(format_ns(fh::Nanos{45600000}) == "45.6 ms");
  REQUIRE(format_ns(fh::Nanos{1200000000}) == "1.20 s");
  REQUIRE(format_ns(fh::Nanos{-1500}) == "-1.50 us");
}

TEST_CASE("rates get a unit", "[bench][report]") {
  REQUIRE(format_rate(500) == "500 /s");
  REQUIRE(format_rate(812000) == "812 k/s");
  REQUIRE(format_rate(1430000) == "1.43 M/s");
}

TEST_CASE("a summary quotes the histogram's percentiles", "[bench][report]") {
  LogLinearHistogram h;
  for (std::int64_t v = 1; v <= 100; ++v) h.record(v);
  const Summary s = summarize(h);
  REQUIRE(s.count == 100);
  REQUIRE(s.min == 1);
  REQUIRE(s.max == 100);
  REQUIRE(s.p50 == 50);
  REQUIRE(s.p90 == 90);
  REQUIRE(s.p99 == 99);
  REQUIRE(s.mean == 50.5);
  REQUIRE(s.p999 == 100);
}

TEST_CASE("an empty histogram summarises to zeros", "[bench][report]") {
  const Summary s = summarize(LogLinearHistogram{});
  REQUIRE(s.count == 0);
  REQUIRE(s.max == 0);
  REQUIRE(s.p99 == 0);
}

TEST_CASE("a table row has the same columns as its header", "[bench][report]") {
  LogLinearHistogram h;
  h.record(1500);
  const std::string header = percentile_header();
  const std::string row = percentile_row("handler", summarize(h));
  REQUIRE(header.size() == row.size());
  REQUIRE(row.starts_with("handler"));
  REQUIRE(row.find("1.50 us") != std::string::npos);
  REQUIRE(header.find("p99.9") != std::string::npos);
}

TEST_CASE("the environment says what it can and measures the clock", "[bench][environment]") {
  const Environment e = collect_environment();
  REQUIRE_FALSE(e.cpu_model.empty());
  REQUIRE(e.logical_cpus >= 1);
  REQUIRE(e.kernel.starts_with("Linux"));
  REQUIRE_FALSE(e.architecture.empty());
  REQUIRE_FALSE(e.compiler.empty());
  REQUIRE_FALSE(e.build_type.empty());
  REQUIRE(e.clock_period >= 1);
  REQUIRE(e.clock_read_cost > 0.0);
  REQUIRE(e.clock_read_cost <
          10000.0);  // a reading that costs 10 us means something is badly wrong
}

TEST_CASE("the rendered environment names every field", "[bench][environment]") {
  const std::string text = render(collect_environment());
  for (const char* label :
       {"cpu", "logical cpus", "cpu limit", "kernel", "compiler", "build type", "clock"}) {
    REQUIRE(text.find(label) != std::string::npos);
  }
  REQUIRE(text.starts_with("environment:"));
}

TEST_CASE("pinning to a cpu is reported, and a nonsense cpu is refused", "[bench][environment]") {
  REQUIRE_FALSE(fh::bench::pin_to_cpu(-1));
  REQUIRE_FALSE(fh::bench::pin_to_cpu(100000));
  cpu_set_t original;
  CPU_ZERO(&original);
  REQUIRE(::sched_getaffinity(0, sizeof original, &original) == 0);
  int first = -1;
  for (std::size_t c = 0; c < CPU_SETSIZE && first < 0; ++c) {
    if (CPU_ISSET(c, &original)) first = static_cast<int>(c);
  }
  REQUIRE(first >= 0);
  // Pinning to a CPU we are already allowed to run on must work, and be reported.
  REQUIRE(fh::bench::pin_to_cpu(first));
  REQUIRE(fh::bench::collect_environment().pinned_cpu == first);
  REQUIRE(::sched_setaffinity(0, sizeof original, &original) == 0);  // put it back for other tests
}

TEST_CASE("the spread of runs is their minimum, median and maximum", "[bench][report]") {
  const Spread s = spread_of({5.0, 1.0, 9.0, 3.0, 7.0});
  REQUIRE(s.min == 1.0);
  REQUIRE(s.median == 5.0);
  REQUIRE(s.max == 9.0);
  REQUIRE(spread_of({2.0, 4.0}).median == 3.0);
  REQUIRE(spread_of({}).max == 0.0);
  REQUIRE(spread_of({7.0}).median == 7.0);
}
