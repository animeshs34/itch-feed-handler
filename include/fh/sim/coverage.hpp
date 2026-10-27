#ifndef FH_SIM_COVERAGE_HPP_
#define FH_SIM_COVERAGE_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include "fh/sim/checker.hpp"

namespace fh::sim {

// What each probe is for.
enum class Expect : std::uint8_t {
  Reached,  // the campaign is only convincing if this happened in some scenario
  Zero,     // this must never happen on a valid ground truth
  Info,     // reported, with no expectation
};

struct CoverageRow {
  std::string name;
  Expect expect{Expect::Info};
  std::uint64_t scenarios{0};  // in how many scenarios the probe was above zero
  std::uint64_t total{0};      // the sum over all scenarios
};

// A table of "did the campaign actually exercise this?". Faults that never produce
// the behaviour they are meant to produce would make a clean result mean nothing, so
// the table lists the interesting behaviours (gaps of each reason, mismatches, stale
// snapshots, every feed health, ...) with how many scenarios reached each.
class Coverage {
 public:
  Coverage();

  // Adds one scenario's result.
  void add(const Verdict& verdict);

  [[nodiscard]] std::uint64_t scenarios() const noexcept { return scenarios_; }
  [[nodiscard]] const std::vector<CoverageRow>& rows() const noexcept { return rows_; }

  // Probes that were expected to be reached but never were.
  [[nodiscard]] std::vector<std::string> unreached() const;
  // Probes that must stay at zero but did not.
  [[nodiscard]] std::vector<std::string> violated() const;

  // A fixed-width text table.
  [[nodiscard]] std::string render() const;

 private:
  std::uint64_t scenarios_{0};
  std::vector<CoverageRow> rows_;
};

}  // namespace fh::sim

#endif  // FH_SIM_COVERAGE_HPP_
