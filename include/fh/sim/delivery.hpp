#ifndef FH_SIM_DELIVERY_HPP_
#define FH_SIM_DELIVERY_HPP_

#include <cstdint>
#include <vector>

#include "fh/sim/faults.hpp"
#include "fh/sim/trace.hpp"
#include "fh/sim/wire.hpp"
#include "prng.hpp"

namespace fh::sim {

// A datagram on its way to the handler, with what the later fault steps need to
// know about it.
struct Arrival {
  Entry entry;
  DatagramKind dkind{DatagramKind::Messages};
  Seq first_seq{0};
  std::uint16_t count{0};
  // True while the bytes are exactly what the publisher sent. A cut-short or altered
  // copy is not genuine, and neither is an invention such as a wrong-session datagram.
  bool genuine{true};
};

// A stretch of sending time during which every datagram is lost on BOTH lines. A burst
// strikes wherever it is placed, even in the part of the day that is otherwise clean.
struct Burst {
  Nanos from{0};
  Nanos to{0};
};

// Picks the bursts for a day. Each message interval before `faulty_until` starts a
// burst with chance `burst_pct`; a burst lasts 1..burst_max intervals.
[[nodiscard]] std::vector<Burst> make_bursts(const FaultConfig& faults, Nanos faulty_until,
                                             Nanos spacing, fhtest::SplitMix64& rng);

// Takes the datagrams of one line to the handler: some are lost, each is delayed by
// a random jitter (line B by `skew_b` more), and some arrive twice.
//
// Only datagrams sent before `faulty_until` can be lost at random or doubled; bursts
// strike wherever they are.
[[nodiscard]] std::vector<Arrival> deliver_line(const std::vector<Datagram>& datagrams,
                                                const FaultConfig& faults, Nanos faulty_until,
                                                const std::vector<Burst>& bursts,
                                                fhtest::SplitMix64& rng);

}  // namespace fh::sim

#endif  // FH_SIM_DELIVERY_HPP_
