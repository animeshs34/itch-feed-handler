#ifndef FH_BENCH_REPORT_HPP_
#define FH_BENCH_REPORT_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include "fh/bench/histogram.hpp"
#include "fh/time.hpp"

namespace fh::bench {

// The numbers a report quotes from a histogram of nanoseconds.
struct Summary {
  std::uint64_t count{0};
  Nanos min{0};
  double mean{0};
  Nanos p50{0};
  Nanos p90{0};
  Nanos p99{0};
  Nanos p999{0};
  Nanos p9999{0};
  Nanos max{0};
};

[[nodiscard]] Summary summarize(const LogLinearHistogram& histogram);

// Minimum, median and maximum of a set of numbers, one per independent run. A single run is
// never a headline number; the spread says how much the runs disagree.
struct Spread {
  double min{0};
  double median{0};
  double max{0};
};

[[nodiscard]] Spread spread_of(std::vector<double> values);

// A duration with a unit chosen to keep three or four significant digits: "850 ns",
// "12.3 us", "4.56 ms", "1.20 s".
[[nodiscard]] std::string format_ns(Nanos value);
[[nodiscard]] std::string format_ns(double value);

// A rate with a unit: "812 k/s", "1.43 M/s".
[[nodiscard]] std::string format_rate(double per_second);

// One line of a table: label, then count and the percentiles.
[[nodiscard]] std::string percentile_header();
[[nodiscard]] std::string percentile_row(const std::string& label, const Summary& summary);

}  // namespace fh::bench

#endif  // FH_BENCH_REPORT_HPP_
