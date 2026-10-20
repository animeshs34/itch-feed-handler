#ifndef FH_TESTS_SUPPORT_EVENT_GEN_HPP_
#define FH_TESTS_SUPPORT_EVENT_GEN_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "fh/events.hpp"
#include "prng.hpp"

namespace fhtest {

struct GenConfig {
  std::uint16_t instruments{3};
  std::size_t events{200};
};

// Makes a seeded stream of VALID book events: every Reduce, Delete and Replace
// refers to an order that is live at that point, and never removes more shares
// than the order has. It keeps its own simple list of live orders to guarantee
// that, and knows nothing about fh::InstrumentBook.
inline std::vector<fh::BookEvent> generate_valid_events(std::uint64_t seed,
                                                        const GenConfig& config) {
  struct Live {
    fh::OrderId id;
    fh::InstrumentId inst;
    fh::Shares shares;
  };

  SplitMix64 rng(seed);
  std::vector<Live> live;
  std::vector<fh::BookEvent> out;
  fh::OrderId next_id = 1;
  fh::Seq seq = 1;

  while (out.size() < config.events) {
    const std::uint64_t roll = rng.below(100);
    if (live.empty() || roll < 40) {
      const auto inst = static_cast<fh::InstrumentId>(1 + rng.below(config.instruments));
      const fh::Side side = rng.below(2) == 0 ? fh::Side::Buy : fh::Side::Sell;
      const fh::Price price = rng.between(95, 105);
      const auto shares = static_cast<fh::Shares>(rng.between(1, 100));
      out.push_back(fh::make_add(seq++, inst, next_id, side, price, shares));
      live.push_back(Live{next_id, inst, shares});
      ++next_id;
    } else {
      const std::size_t index = static_cast<std::size_t>(rng.below(live.size()));
      Live& target = live[index];
      if (roll < 65) {
        const auto amount =
            static_cast<fh::Shares>(rng.between(1, static_cast<std::int64_t>(target.shares)));
        out.push_back(fh::make_reduce(seq++, target.inst, target.id, amount));
        target.shares -= amount;
        if (target.shares == 0) {
          live[index] = live.back();
          live.pop_back();
        }
      } else if (roll < 80) {
        out.push_back(fh::make_delete(seq++, target.inst, target.id));
        live[index] = live.back();
        live.pop_back();
      } else {
        const fh::Price price = rng.between(95, 105);
        const auto shares = static_cast<fh::Shares>(rng.between(1, 100));
        out.push_back(fh::make_replace(seq++, target.inst, target.id, next_id, price, shares));
        target.id = next_id++;
        target.shares = shares;
      }
    }
  }
  return out;
}

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_EVENT_GEN_HPP_
