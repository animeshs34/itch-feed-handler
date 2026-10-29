#include "fh/bench/histogram.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>

namespace fh::bench {

LogLinearHistogram::LogLinearHistogram(unsigned precision_bits, unsigned max_value_bits)
    : precision_bits_(precision_bits), max_value_bits_(max_value_bits) {
  assert(precision_bits >= 1 && precision_bits <= 16);
  assert(max_value_bits > precision_bits + 1 && max_value_bits <= 62);
  // The largest value that is not clamped is 2^max_value_bits - 1.
  counts_.assign(index_of((std::uint64_t{1} << max_value_bits) - 1) + 1, 0);
}

std::size_t LogLinearHistogram::index_of(std::uint64_t value) const noexcept {
  if (value < (std::uint64_t{2} << precision_bits_)) return static_cast<std::size_t>(value);
  const unsigned exponent = 63U - static_cast<unsigned>(std::countl_zero(value));
  const unsigned shift = exponent - precision_bits_;
  return (static_cast<std::size_t>(shift) << precision_bits_) +
         static_cast<std::size_t>(value >> shift);
}

std::uint64_t LogLinearHistogram::highest_in(std::size_t index) const noexcept {
  if (index < (std::size_t{2} << precision_bits_)) return index;
  const unsigned shift = static_cast<unsigned>(index >> precision_bits_) - 1;
  const std::uint64_t mantissa =
      (index & ((std::size_t{1} << precision_bits_) - 1)) + (std::uint64_t{1} << precision_bits_);
  return ((mantissa + 1) << shift) - 1;
}

void LogLinearHistogram::record(std::int64_t value, std::uint64_t times) noexcept {
  if (times == 0) return;
  if (value < 0) value = 0;
  std::uint64_t v = static_cast<std::uint64_t>(value);
  const std::uint64_t limit = (std::uint64_t{1} << max_value_bits_) - 1;
  if (v > limit) {
    v = limit;
    overflow_ += times;
  }
  counts_[index_of(v)] += times;
  if (count_ == 0 || value < min_) min_ = value;
  if (count_ == 0 || value > max_) max_ = value;
  count_ += times;
  sum_ += static_cast<long double>(value) * static_cast<long double>(times);
}

double LogLinearHistogram::mean() const noexcept {
  return count_ == 0 ? 0.0 : static_cast<double>(sum_ / static_cast<long double>(count_));
}

std::int64_t LogLinearHistogram::value_at_percentile(double p) const noexcept {
  if (count_ == 0) return 0;
  p = std::clamp(p, 0.0, 100.0);
  if (p <= 0.0) return min_;
  if (p >= 100.0) return max_;
  // The rank of the value we want, counting from 1.
  const auto rank =
      static_cast<std::uint64_t>(std::max(1.0, std::ceil(p / 100.0 * static_cast<double>(count_))));
  std::uint64_t seen = 0;
  for (std::size_t i = 0; i < counts_.size(); ++i) {
    seen += counts_[i];
    if (seen >= rank) {
      const auto top = static_cast<std::int64_t>(
          std::min<std::uint64_t>(highest_in(i), static_cast<std::uint64_t>(max_)));
      return std::max(top, min_);
    }
  }
  return max_;
}

void LogLinearHistogram::merge(const LogLinearHistogram& other) {
  assert(precision_bits_ == other.precision_bits_ && max_value_bits_ == other.max_value_bits_);
  for (std::size_t i = 0; i < counts_.size(); ++i) counts_[i] += other.counts_[i];
  if (other.count_ != 0) {
    if (count_ == 0 || other.min_ < min_) min_ = other.min_;
    if (count_ == 0 || other.max_ > max_) max_ = other.max_;
  }
  count_ += other.count_;
  overflow_ += other.overflow_;
  sum_ += other.sum_;
}

void LogLinearHistogram::reset() noexcept {
  std::fill(counts_.begin(), counts_.end(), 0);
  count_ = overflow_ = 0;
  min_ = max_ = 0;
  sum_ = 0;
}

}  // namespace fh::bench
