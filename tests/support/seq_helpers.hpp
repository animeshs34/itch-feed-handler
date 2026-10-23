#ifndef FH_TESTS_SUPPORT_SEQ_HELPERS_HPP_
#define FH_TESTS_SUPPORT_SEQ_HELPERS_HPP_

#include <variant>
#include <vector>

#include "fh/sequencer.hpp"
#include "packets.hpp"

namespace fhtest {

inline constexpr fh::Nanos kUnit = fh::kMillisecond;

inline fh::SequencerConfig sequencer_config(fh::Nanos skew = 3 * kUnit,
                                            std::size_t capacity = 100) {
  fh::SequencerConfig c;
  c.skew_window = skew;
  c.buffer_capacity = capacity;
  return c;
}

// A message that uses its sequence number and changes no book. The digest is
// the number itself; these tests are about order, not content.
inline fh::SequencedMessage msg(fh::Seq seq) { return no_effect(seq, seq); }

// The sequencer's output split into its two kinds of item.
struct Flat {
  std::vector<fh::Seq> messages;
  std::vector<fh::GapDeclared> gaps;
};

inline Flat flatten(const std::vector<fh::SeqEvent>& events) {
  Flat flat;
  for (const fh::SeqEvent& e : events) {
    if (const auto* m = std::get_if<fh::SequencedMessage>(&e)) flat.messages.push_back(m->seq);
    if (const auto* g = std::get_if<fh::GapDeclared>(&e)) flat.gaps.push_back(*g);
  }
  return flat;
}

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_SEQ_HELPERS_HPP_
