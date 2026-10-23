#include "fh/sequencer.hpp"

#include <algorithm>
#include <cassert>
#include <utility>

#include "fh/seeded_bug.hpp"

namespace fh {

Sequencer::Sequencer(const SequencerConfig& config, Seq first_expected)
    : config_(config), expected_(first_expected) {}

bool Sequencer::has_hole() const noexcept {
  return !buffer_.empty() || (tail_.has_value() && *tail_ >= expected_);
}

void Sequencer::accept(SequencedMessage message, Nanos t, std::vector<SeqEvent>& out) {
  const Seq seq = message.seq;
  if (seq < expected_) {
    ++late_after_gap_;  // arrived after we gave up on it; a snapshot will cover it
    return;
  }
  if (seq == expected_) {
    out.emplace_back(std::move(message));
    ++expected_;
    drain(out);
    settle(t);
    return;
  }
  // Ahead of what we expect: hold it and start waiting for the missing numbers.
  if (!buffer_.emplace(seq, std::move(message)).second) return;
  if (!gap_since_) gap_since_ = t;
  while (buffer_.size() > config_.buffer_capacity) {
    declare_gap(GapReason::Overflow, t, out);
  }
}

void Sequencer::note_next_expected(Seq next_seq, Nanos t) {
  if (next_seq == 0 || FH_BUG(5)) return;
  const Seq last_sent = next_seq - 1;
  if (last_sent < expected_) return;  // nothing is missing
  tail_ = std::max(tail_.value_or(0), last_sent);
  if (!gap_since_) gap_since_ = t;
}

void Sequencer::tick(Nanos t, std::vector<SeqEvent>& out) {
  if (gap_since_ && t - *gap_since_ >= config_.skew_window) {
    declare_gap(buffer_.empty() ? GapReason::Tail : GapReason::Timeout, t, out);
  }
}

void Sequencer::drain(std::vector<SeqEvent>& out) {
  while (!buffer_.empty() && buffer_.begin()->first == expected_) {
    out.emplace_back(std::move(buffer_.begin()->second));
    buffer_.erase(buffer_.begin());
    ++expected_;
  }
}

void Sequencer::settle(Nanos t) {
  if (tail_ && *tail_ < expected_) tail_.reset();
  if (has_hole()) {
    gap_since_ = t;  // progress was made; a new wait starts for what is still missing
  } else {
    gap_since_.reset();
  }
}

void Sequencer::declare_gap(GapReason reason, Nanos t, std::vector<SeqEvent>& out) {
  Seq to = 0;
  if (reason == GapReason::Tail) {
    assert(tail_.has_value());
    to = *tail_;
  } else {
    assert(!buffer_.empty());
    to = buffer_.begin()->first - 1;  // up to the number before the first one we hold
  }
  out.emplace_back(GapDeclared{expected_, FH_BUG(4) && to > expected_ ? to - 1 : to, reason});
  expected_ = to + 1;
  drain(out);
  settle(t);
}

}  // namespace fh
