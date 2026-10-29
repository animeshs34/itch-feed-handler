#ifndef FH_BENCH_HISTOGRAM_HPP_
#define FH_BENCH_HISTOGRAM_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fh::bench {

// A histogram of non-negative integers (nanoseconds, here) whose buckets are exact for
// small values and grow geometrically for large ones, so that the error of any reported
// percentile is a fixed fraction of its value: at most 2^-precision_bits (0.78 percent for
// the default 7 bits) whether the value is 50 ns or 5 seconds. It is the same idea as
// Gil Tene's HdrHistogram, written out here in about eighty lines.
//
// Recording is a shift, an add and an increment, with no allocation after construction, so
// it can be done inside a measured loop.
//
// Values below 2^(precision_bits + 1) get a bucket each. A larger value v, with
// e = floor(log2 v) and shift = e - precision_bits, goes to bucket
// (shift << precision_bits) + (v >> shift): the top precision_bits + 1 bits of the value.
class LogLinearHistogram {
 public:
  // `max_value_bits`: values of 2^max_value_bits or more are clamped to the largest bucket
  // and counted in overflow(). 44 bits is about 4.9 hours in nanoseconds.
  explicit LogLinearHistogram(unsigned precision_bits = 7, unsigned max_value_bits = 44);

  // Records `value` (negative values count as 0) `times` times.
  void record(std::int64_t value, std::uint64_t times = 1) noexcept;

  [[nodiscard]] std::uint64_t count() const noexcept { return count_; }
  [[nodiscard]] std::uint64_t overflow() const noexcept { return overflow_; }
  [[nodiscard]] std::int64_t min() const noexcept { return count_ == 0 ? 0 : min_; }
  [[nodiscard]] std::int64_t max() const noexcept { return count_ == 0 ? 0 : max_; }
  [[nodiscard]] double mean() const noexcept;

  // The smallest value v such that at least p percent of the recorded values are at most v,
  // to the histogram's precision. `p` is clamped to [0, 100]. p = 0 gives min() and
  // p = 100 gives max(), both exact. An empty histogram gives 0. The answer is the top of
  // its bucket (never an underestimate), and never above max().
  [[nodiscard]] std::int64_t value_at_percentile(double p) const noexcept;

  // Adds another histogram's counts. Both must have been built with the same parameters.
  void merge(const LogLinearHistogram& other);

  void reset() noexcept;

  [[nodiscard]] unsigned precision_bits() const noexcept { return precision_bits_; }
  [[nodiscard]] unsigned max_value_bits() const noexcept { return max_value_bits_; }

 private:
  [[nodiscard]] std::size_t index_of(std::uint64_t value) const noexcept;
  [[nodiscard]] std::uint64_t highest_in(std::size_t index) const noexcept;

  unsigned precision_bits_;
  unsigned max_value_bits_;
  std::vector<std::uint64_t> counts_;
  std::uint64_t count_{0};
  std::uint64_t overflow_{0};
  std::int64_t min_{0};
  std::int64_t max_{0};
  long double sum_{0};
};

}  // namespace fh::bench

#endif  // FH_BENCH_HISTOGRAM_HPP_
