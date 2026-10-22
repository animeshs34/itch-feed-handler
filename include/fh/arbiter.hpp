#ifndef FH_ARBITER_HPP_
#define FH_ARBITER_HPP_

#include <cstddef>
#include <cstdint>
#include <map>

#include "fh/types.hpp"

namespace fh {

enum class Verdict : std::uint8_t {
  New,                  // first time this sequence number has been seen
  Duplicate,            // seen before, identical bytes
  DuplicateUnverified,  // so old that the first copy has been forgotten, so it cannot be compared
  Mismatch,             // seen before, but the bytes differ
};

// Decides, for every message that arrives on either line, whether it is the
// first copy. The first copy wins; later copies are only compared and dropped.
//
// It remembers the fingerprint of recent sequence numbers, bounded by a window
// so memory cannot grow without limit.
class Arbiter {
 public:
  // `window`: how many sequence numbers below the next expected one to remember.
  explicit Arbiter(std::size_t window) : window_(window) {}

  [[nodiscard]] Verdict observe(Seq seq, std::uint64_t digest);

  // Tells the arbiter the next sequence number the sequencer is waiting for, so
  // it can forget fingerprints older than the window.
  void advance(Seq expected);

  [[nodiscard]] std::size_t remembered() const noexcept { return seen_.size(); }

 private:
  std::size_t window_;
  std::map<Seq, std::uint64_t> seen_;
  Seq floor_{0};  // every sequence number <= floor_ has been forgotten
};

}  // namespace fh

#endif  // FH_ARBITER_HPP_
