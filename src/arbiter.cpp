#include "fh/arbiter.hpp"

#include <algorithm>
#include <bit>

#include "fh/seeded_bug.hpp"

namespace fh {

namespace {
// Room beyond the window for numbers that arrive a little ahead of the expected one.
constexpr std::size_t kSlack = 1024;
}  // namespace

Arbiter::Arbiter(std::size_t window)
    : window_(window),
      ring_(std::bit_ceil(window + 1 + kSlack)),
      mask_(static_cast<Seq>(ring_.size() - 1)) {}

Verdict Arbiter::observe(Seq seq, std::uint64_t digest) {
  if (seq <= floor_) return Verdict::DuplicateUnverified;
  std::uint64_t existing = 0;
  if (seq - floor_ <= ring_.size()) {
    Slot& slot = ring_[seq & mask_];
    if (slot.seq != seq) {
      slot = Slot{seq, digest};
      ++in_ring_;
      return Verdict::New;
    }
    existing = slot.digest;
  } else {
    const auto [it, inserted] = overflow_.emplace(seq, digest);
    if (inserted) return Verdict::New;
    existing = it->second;
  }
  return (existing == digest || FH_BUG(7)) ? Verdict::Duplicate : Verdict::Mismatch;
}

void Arbiter::advance(Seq expected) {
  if (expected <= window_) return;  // nothing is old enough to forget yet
  const Seq keep_from = expected - window_;
  const Seq new_floor = std::max(floor_, keep_from - 1);
  if (new_floor == floor_) return;

  // Ring entries all lie in (floor_, floor_ + size], so only that stretch can hold
  // something that is now too old.
  const Seq last = std::min(new_floor, floor_ + ring_.size());
  for (Seq s = floor_ + 1; s <= last; ++s) {
    Slot& slot = ring_[s & mask_];
    if (slot.seq == s) {
      slot.seq = 0;
      --in_ring_;
    }
  }
  overflow_.erase(overflow_.begin(), overflow_.upper_bound(new_floor));
  floor_ = new_floor;
  if (!overflow_.empty()) migrate_overflow();
}

// The floor moved, so the ring reaches further: bring in what now fits.
void Arbiter::migrate_overflow() {
  while (!overflow_.empty() && overflow_.begin()->first - floor_ <= ring_.size()) {
    const auto first = overflow_.begin();
    ring_[first->first & mask_] = Slot{first->first, first->second};
    ++in_ring_;
    overflow_.erase(first);
  }
}

}  // namespace fh
