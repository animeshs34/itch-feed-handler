#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

#include "fh/bench/histogram.hpp"
#include "prng.hpp"

using fh::bench::LogLinearHistogram;

TEST_CASE("an empty histogram answers zero to everything", "[bench][histogram]") {
  const LogLinearHistogram h;
  REQUIRE(h.count() == 0);
  REQUIRE(h.min() == 0);
  REQUIRE(h.max() == 0);
  REQUIRE(h.mean() == 0.0);
  REQUIRE(h.value_at_percentile(50) == 0);
  REQUIRE(h.value_at_percentile(100) == 0);
}

TEST_CASE("small values are exact", "[bench][histogram]") {
  LogLinearHistogram h;  // exact below 2^8 = 256
  for (std::int64_t v = 0; v < 256; ++v) h.record(v);
  REQUIRE(h.count() == 256);
  REQUIRE(h.min() == 0);
  REQUIRE(h.max() == 255);
  REQUIRE(h.value_at_percentile(50) == 127);  // the 128th of 256 values, counting from 0
  REQUIRE(h.value_at_percentile(100) == 255);
  REQUIRE(h.value_at_percentile(0) == 0);
  REQUIRE(h.value_at_percentile(25) == 63);
}

TEST_CASE("a single value is every percentile", "[bench][histogram]") {
  LogLinearHistogram h;
  h.record(123456);
  for (const double p : {0.0, 1.0, 50.0, 99.9, 100.0}) {
    const auto v = h.value_at_percentile(p);
    REQUIRE(static_cast<double>(v) >= 123456.0 * 0.992);
    REQUIRE(v <= 123456);  // never above the maximum seen
  }
  REQUIRE(h.min() == 123456);
  REQUIRE(h.max() == 123456);
}

TEST_CASE("a percentile is within the stated precision of the true one", "[bench][histogram]") {
  fhtest::SplitMix64 rng(1);
  LogLinearHistogram h;  // 7 bits: relative error at most 1/128
  std::vector<std::int64_t> values;
  for (int i = 0; i < 200000; ++i) {
    // Values spread over seven orders of magnitude: 100 ns to 1 s.
    const double exponent = 2.0 + 7.0 * static_cast<double>(rng.below(1000000)) / 1000000.0;
    const auto v = static_cast<std::int64_t>(std::pow(10.0, exponent));
    values.push_back(v);
    h.record(v);
  }
  std::sort(values.begin(), values.end());
  for (const double p : {1.0, 10.0, 50.0, 90.0, 99.0, 99.9, 99.99}) {
    const auto rank =
        static_cast<std::size_t>(std::ceil(p / 100.0 * static_cast<double>(values.size())));
    const double truth = static_cast<double>(values[rank - 1]);
    const double got = static_cast<double>(h.value_at_percentile(p));
    INFO("p " << p << " truth " << truth << " got " << got);
    REQUIRE(got >= truth);  // never an underestimate
    REQUIRE(got <= truth * (1.0 + 1.0 / 128.0) + 1);
  }
  REQUIRE(h.value_at_percentile(100) == values.back());
  REQUIRE(h.min() == values.front());
}

TEST_CASE("more precision bits give a smaller error", "[bench][histogram]") {
  LogLinearHistogram coarse(3, 40);  // up to 12.5 percent
  LogLinearHistogram fine(10, 40);   // under 0.1 percent
  for (std::int64_t v = 1000; v < 5000000; v += 997) {
    coarse.record(v);
    fine.record(v);
  }
  const double c = static_cast<double>(coarse.value_at_percentile(50));
  const double f = static_cast<double>(fine.value_at_percentile(50));
  REQUIRE(f <= c);
  REQUIRE(c / f > 1.001);  // they really differ
  REQUIRE(c / f < 1.13);
}

TEST_CASE("the bucket tops tile the number line: every value is at most its bucket's top",
          "[bench][histogram]") {
  // value_at_percentile(100) is exact, so probe each value alone through p = 100 of a
  // histogram with one extra, larger value, and p just below it.
  const std::vector<std::int64_t> probes = {
      255, 256, 257, 511, 512, 1000, 65535, 65536, 1000000, 123456789, std::int64_t{1} << 40};
  for (const std::int64_t v : probes) {
    LogLinearHistogram h;
    h.record(v);
    h.record(v + 1000000000000LL);
    const auto top = h.value_at_percentile(50);
    REQUIRE(top >= v);
    REQUIRE(static_cast<double>(top) <= static_cast<double>(v) * (1.0 + 1.0 / 128.0) + 1);
  }
}

TEST_CASE("recording a count at once equals recording one at a time", "[bench][histogram]") {
  LogLinearHistogram a, b;
  a.record(5000, 100);
  a.record(70000, 3);
  for (int i = 0; i < 100; ++i) b.record(5000);
  for (int i = 0; i < 3; ++i) b.record(70000);
  REQUIRE(a.count() == b.count());
  for (const double p : {1.0, 50.0, 97.0, 99.0, 100.0})
    REQUIRE(a.value_at_percentile(p) == b.value_at_percentile(p));
  REQUIRE(a.mean() == b.mean());
  a.record(5, 0);
  REQUIRE(a.count() == 103);
}

TEST_CASE("negative values count as zero and huge values are clamped and counted",
          "[bench][histogram]") {
  LogLinearHistogram h(7, 20);  // values up to 2^20 - 1
  h.record(-5);
  REQUIRE(h.min() == 0);
  h.record(std::int64_t{1} << 40);
  REQUIRE(h.overflow() == 1);
  REQUIRE(h.count() == 2);
  REQUIRE(h.max() == (std::int64_t{1} << 40));  // the true maximum is still remembered
  REQUIRE(h.value_at_percentile(99) <= (std::int64_t{1} << 40));
}

TEST_CASE("merging two histograms equals recording everything in one", "[bench][histogram]") {
  fhtest::SplitMix64 rng(2);
  LogLinearHistogram a, b, whole;
  for (int i = 0; i < 20000; ++i) {
    const auto v = static_cast<std::int64_t>(rng.below(5000000));
    (i % 3 == 0 ? a : b).record(v);
    whole.record(v);
  }
  a.merge(b);
  REQUIRE(a.count() == whole.count());
  REQUIRE(a.min() == whole.min());
  REQUIRE(a.max() == whole.max());
  for (const double p : {1.0, 50.0, 90.0, 99.0, 99.9})
    REQUIRE(a.value_at_percentile(p) == whole.value_at_percentile(p));
  REQUIRE(std::abs(a.mean() - whole.mean()) < 1e-6 * whole.mean());
}

TEST_CASE("reset empties the histogram", "[bench][histogram]") {
  LogLinearHistogram h;
  h.record(10);
  h.record(20);
  h.reset();
  REQUIRE(h.count() == 0);
  REQUIRE(h.max() == 0);
  h.record(7);
  REQUIRE(h.min() == 7);
  REQUIRE(h.max() == 7);
}

TEST_CASE("the mean is exact", "[bench][histogram]") {
  LogLinearHistogram h;
  for (std::int64_t v = 1; v <= 1000; ++v) h.record(v * 1000);
  REQUIRE(std::abs(h.mean() - 500500.0) < 1e-6);
}

TEST_CASE("percentiles never decrease as p grows", "[bench][histogram]") {
  fhtest::SplitMix64 rng(3);
  LogLinearHistogram h;
  for (int i = 0; i < 10000; ++i) h.record(static_cast<std::int64_t>(rng.below(1u << 30)));
  std::int64_t previous = -1;
  for (double p = 0; p <= 100; p += 0.5) {
    const auto v = h.value_at_percentile(p);
    REQUIRE(v >= previous);
    previous = v;
  }
}
