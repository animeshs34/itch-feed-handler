#ifndef FH_ARBITER_HPP_
#define FH_ARBITER_HPP_

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

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
//
// Storage: the numbers it has to remember lie in (floor_, floor_ + window + slack], so
// they sit in a ring indexed by `seq & mask` (no allocation, no tree). A number further
// ahead than the ring reaches (a far-future packet) goes to a small ordered overflow map
// and moves into the ring once the floor catches up. The answers are the same as with one
// ordered map.
class Arbiter {
 public:
  // `window`: how many sequence numbers below the next expected one to remember.
  explicit Arbiter(std::size_t window);

  [[nodiscard]] Verdict observe(Seq seq, std::uint64_t digest);

  // Tells the arbiter the next sequence number the sequencer is waiting for, so
  // it can forget fingerprints older than the window.
  void advance(Seq expected);

  [[nodiscard]] std::size_t remembered() const noexcept { return in_ring_ + overflow_.size(); }

 private:
  struct Slot {
    Seq seq{0};  // 0 = empty; sequence numbers start at 1
    std::uint64_t digest{0};
  };

  void migrate_overflow();

  std::size_t window_;
  std::vector<Slot> ring_;
  Seq mask_;  // ring_.size() - 1; the size is a power of two
  std::size_t in_ring_{0};
  std::map<Seq, std::uint64_t> overflow_;  // numbers beyond the ring's reach
  Seq floor_{0};                           // every sequence number <= floor_ has been forgotten
};

}  // namespace fh

#endif  // FH_ARBITER_HPP_
