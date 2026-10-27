#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <initializer_list>

#include "fh/sim/scenario.hpp"
#include "fh/sim/shrink.hpp"

namespace {

// A trace of n ticks at times 1..n, enough to test the shrinking logic by itself.
fh::sim::Trace ticks(std::size_t n) {
  fh::sim::Trace t;
  for (std::size_t i = 1; i <= n; ++i) {
    t.entries.push_back(
        fh::sim::Entry{static_cast<fh::Nanos>(i), fh::sim::EntryKind::Tick, fh::Line::A, {}});
  }
  return t;
}

bool has_times(const fh::sim::Trace& t, std::initializer_list<fh::Nanos> times) {
  for (const fh::Nanos want : times) {
    const bool found = std::any_of(t.entries.begin(), t.entries.end(),
                                   [want](const fh::sim::Entry& e) { return e.t == want; });
    if (!found) return false;
  }
  return true;
}

}  // namespace

TEST_CASE("it shrinks to exactly the entries the failure needs", "[sim][shrink]") {
  const auto result =
      fh::sim::shrink(ticks(100), [](const fh::sim::Trace& t) { return has_times(t, {17, 63}); });
  REQUIRE(result.entries.size() == 2);
  REQUIRE(has_times(result, {17, 63}));
}

TEST_CASE("it shrinks to a single entry when one is enough", "[sim][shrink]") {
  const auto result =
      fh::sim::shrink(ticks(257), [](const fh::sim::Trace& t) { return has_times(t, {200}); });
  REQUIRE(result.entries.size() == 1);
  REQUIRE(result.entries[0].t == 200);
}

TEST_CASE("the result is 1-minimal: removing any one entry stops the failure", "[sim][shrink]") {
  const auto fails = [](const fh::sim::Trace& t) { return has_times(t, {5, 6, 40}); };
  const auto result = fh::sim::shrink(ticks(80), fails);
  REQUIRE(fails(result));
  for (std::size_t i = 0; i < result.entries.size(); ++i) {
    auto smaller = result;
    smaller.entries.erase(smaller.entries.begin() + static_cast<std::ptrdiff_t>(i));
    REQUIRE_FALSE(fails(smaller));
  }
}

TEST_CASE("a trace that does not fail is returned whole", "[sim][shrink]") {
  const auto original = ticks(30);
  const auto result = fh::sim::shrink(original, [](const fh::sim::Trace&) { return false; });
  REQUIRE(result == original);
}

TEST_CASE("the budget bounds the number of replays", "[sim][shrink]") {
  std::size_t calls = 0;
  const auto result = fh::sim::shrink(
      ticks(500),
      [&calls](const fh::sim::Trace& t) {
        ++calls;
        return has_times(t, {3, 499});
      },
      20);
  REQUIRE(calls <= 20);
  REQUIRE(has_times(result, {3, 499}));  // whatever it returns still fails
}

TEST_CASE("shrinking is deterministic and leaves the ground truth alone", "[sim][shrink]") {
  auto trace = ticks(60);
  trace.truth = fh::sim::make_day(1, fh::sim::DayConfig{});
  const auto fails = [](const fh::sim::Trace& t) { return has_times(t, {9, 41}); };
  const auto a = fh::sim::shrink(trace, fails);
  const auto b = fh::sim::shrink(trace, fails);
  REQUIRE(a == b);
  REQUIRE(a.truth == trace.truth);
  REQUIRE(a.core == trace.core);
}
