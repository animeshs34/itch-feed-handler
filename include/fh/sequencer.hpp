#ifndef FH_SEQUENCER_HPP_
#define FH_SEQUENCER_HPP_

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <variant>
#include <vector>

#include "fh/messages.hpp"
#include "fh/outputs.hpp"
#include "fh/time.hpp"
#include "fh/types.hpp"

namespace fh {

struct SequencerConfig {
  // How long to wait for a missing sequence number before giving up on it.
  Nanos skew_window{kMillisecond};
  // How many out-of-order messages may be held. When more would be held, the
  // wait is cut short and a gap is declared.
  std::size_t buffer_capacity{65536};
};

// What the sequencer hands on, in the order it must be processed:
// a message that is next in line, or the announcement that sequence numbers
// were given up on.
using SeqEvent = std::variant<SequencedMessage, GapDeclared>;

// Turns messages that arrive in any order into messages in sequence order.
//
// A message that is ahead of the next expected one is held. If the missing
// numbers do not arrive within the skew window, the sequencer declares a gap
// and carries on after it.
class Sequencer {
 public:
  Sequencer(const SequencerConfig& config, Seq first_expected);

  // Offers a message that the arbiter has called New.
  void accept(SequencedMessage message, Nanos t, std::vector<SeqEvent>& out);

  // A heartbeat or end-of-session packet said the publisher would send
  // `next_seq` next. If that is further ahead than we are, messages are missing
  // at the tail, and the wait for them starts.
  void note_next_expected(Seq next_seq, Nanos t);

  // Time has passed: give up on anything that has been waited for too long.
  void tick(Nanos t, std::vector<SeqEvent>& out);

  [[nodiscard]] const SequencerConfig& config() const noexcept { return config_; }
  [[nodiscard]] Seq expected() const noexcept { return expected_; }
  [[nodiscard]] bool has_hole() const noexcept;
  [[nodiscard]] std::size_t buffered() const noexcept { return buffer_.size(); }
  [[nodiscard]] std::optional<Nanos> waiting_since() const noexcept { return gap_since_; }
  [[nodiscard]] std::uint64_t late_after_gap() const noexcept { return late_after_gap_; }

 private:
  void drain(std::vector<SeqEvent>& out);
  void settle(Nanos t);
  void declare_gap(GapReason reason, Nanos t, std::vector<SeqEvent>& out);

  SequencerConfig config_;
  Seq expected_;
  std::map<Seq, SequencedMessage> buffer_;
  std::optional<Seq> tail_;         // the highest sequence number a heartbeat said exists
  std::optional<Nanos> gap_since_;  // when the current wait for missing numbers began
  std::uint64_t late_after_gap_{0};
};

}  // namespace fh

#endif  // FH_SEQUENCER_HPP_
