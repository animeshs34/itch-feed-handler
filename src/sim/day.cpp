#include "fh/sim/day.hpp"

#include <array>

#include "event_gen.hpp"
#include "prng.hpp"

namespace fh::sim {
namespace {

constexpr std::array<char, 4> kTradeFillers{'P', 'I', 'B', 'N'};

void add_filler(GroundTruth& day, char filler, char code, InstrumentId locate) {
  GroundMessage m;
  m.seq = day.messages.size() + 1;
  m.filler = filler;
  m.code = code;
  m.locate = locate;
  day.messages.push_back(m);
}

}  // namespace

GroundTruth make_day(std::uint64_t seed, const DayConfig& config) {
  fhtest::SplitMix64 rng(seed);
  const std::vector<BookEvent> events = fhtest::generate_valid_events(
      rng.next(), fhtest::GenConfig{config.instruments, config.book_events});

  GroundTruth day;
  add_filler(day, 'S', 'O', 0);
  for (const char kind : {'R', 'H', 'Y'}) {
    for (InstrumentId inst = 1; inst <= config.instruments; ++inst)
      add_filler(day, kind, ' ', inst);
  }
  add_filler(day, 'S', 'S', 0);
  add_filler(day, 'S', 'Q', 0);

  unsigned since_filler = 0;
  for (const BookEvent& original : events) {
    GroundMessage m;
    m.seq = day.messages.size() + 1;
    m.event = original;
    m.event->seq = m.seq;
    day.messages.push_back(m);
    if (config.filler_every != 0 && ++since_filler == config.filler_every) {
      since_filler = 0;
      add_filler(day, kTradeFillers[rng.below(kTradeFillers.size())], ' ', original.inst);
    }
  }

  add_filler(day, 'S', 'M', 0);
  add_filler(day, 'S', 'E', 0);
  add_filler(day, 'S', 'C', 0);
  return day;
}

}  // namespace fh::sim
