#include "fh/bench/report.hpp"

#include <algorithm>
#include <cstdio>

namespace fh::bench {

Summary summarize(const LogLinearHistogram& h) {
  Summary s;
  s.count = h.count();
  s.min = h.min();
  s.mean = h.mean();
  s.p50 = h.value_at_percentile(50);
  s.p90 = h.value_at_percentile(90);
  s.p99 = h.value_at_percentile(99);
  s.p999 = h.value_at_percentile(99.9);
  s.p9999 = h.value_at_percentile(99.99);
  s.max = h.max();
  return s;
}

Spread spread_of(std::vector<double> values) {
  Spread s;
  if (values.empty()) return s;
  std::sort(values.begin(), values.end());
  s.min = values.front();
  s.max = values.back();
  const std::size_t n = values.size();
  s.median = n % 2 == 1 ? values[n / 2] : (values[n / 2 - 1] + values[n / 2]) / 2.0;
  return s;
}

std::string format_ns(double value) {
  char buffer[40];
  const double magnitude = value < 0 ? -value : value;
  if (magnitude < 1e3) {
    std::snprintf(buffer, sizeof buffer, "%.0f ns", value);
  } else if (magnitude < 1e4) {
    std::snprintf(buffer, sizeof buffer, "%.2f us", value / 1e3);
  } else if (magnitude < 1e6) {
    std::snprintf(buffer, sizeof buffer, "%.1f us", value / 1e3);
  } else if (magnitude < 1e7) {
    std::snprintf(buffer, sizeof buffer, "%.2f ms", value / 1e6);
  } else if (magnitude < 1e9) {
    std::snprintf(buffer, sizeof buffer, "%.1f ms", value / 1e6);
  } else {
    std::snprintf(buffer, sizeof buffer, "%.2f s", value / 1e9);
  }
  return buffer;
}

std::string format_ns(Nanos value) { return format_ns(static_cast<double>(value)); }

std::string format_rate(double per_second) {
  char buffer[40];
  if (per_second < 1e3) {
    std::snprintf(buffer, sizeof buffer, "%.0f /s", per_second);
  } else if (per_second < 1e6) {
    std::snprintf(buffer, sizeof buffer, "%.0f k/s", per_second / 1e3);
  } else {
    std::snprintf(buffer, sizeof buffer, "%.2f M/s", per_second / 1e6);
  }
  return buffer;
}

std::string percentile_header() {
  char line[200];
  std::snprintf(line, sizeof line, "%-24s %10s %10s %10s %10s %10s %10s %10s %10s\n", "", "count",
                "min", "p50", "p90", "p99", "p99.9", "p99.99", "max");
  return line;
}

std::string percentile_row(const std::string& label, const Summary& s) {
  char line[300];
  std::snprintf(line, sizeof line, "%-24s %10llu %10s %10s %10s %10s %10s %10s %10s\n",
                label.c_str(), static_cast<unsigned long long>(s.count), format_ns(s.min).c_str(),
                format_ns(s.p50).c_str(), format_ns(s.p90).c_str(), format_ns(s.p99).c_str(),
                format_ns(s.p999).c_str(), format_ns(s.p9999).c_str(), format_ns(s.max).c_str());
  return line;
}

}  // namespace fh::bench
