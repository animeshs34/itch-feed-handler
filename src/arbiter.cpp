#include "fh/arbiter.hpp"

#include <algorithm>

#include "fh/seeded_bug.hpp"

namespace fh {

Verdict Arbiter::observe(Seq seq, std::uint64_t digest) {
  if (seq <= floor_) return Verdict::DuplicateUnverified;
  const auto [it, inserted] = seen_.emplace(seq, digest);
  if (inserted) return Verdict::New;
  return (it->second == digest || FH_BUG(7)) ? Verdict::Duplicate : Verdict::Mismatch;
}

void Arbiter::advance(Seq expected) {
  if (expected <= window_) return;  // nothing is old enough to forget yet
  const Seq keep_from = expected - window_;
  seen_.erase(seen_.begin(), seen_.lower_bound(keep_from));
  floor_ = std::max(floor_, keep_from - 1);
}

}  // namespace fh
