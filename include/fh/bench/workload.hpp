#ifndef FH_BENCH_WORKLOAD_HPP_
#define FH_BENCH_WORKLOAD_HPP_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "fh/nasdaq/itch_encoder.hpp"
#include "fh/sim/ground_truth.hpp"

namespace fh::bench {

// A clean day, built in advance as the datagrams of one line, so that a benchmark measures
// the handler and not the generator. No heartbeats, no faults: every message arrives once
// and in order.
struct Workload {
  std::vector<nasdaq::Bytes>
      datagrams;  // in order; datagram i holds messages starting at first_seq[i]
  std::vector<std::uint64_t> first_seq;
  std::uint64_t messages{0};
  std::size_t instruments{0};
  std::uint64_t seed{0};
  unsigned max_pack{1};
  std::uint64_t bytes{0};  // total datagram bytes
};

// `book_events` book-changing messages over `instruments` instruments plus the usual
// housekeeping messages, packed into datagrams of 1 to `max_pack` messages. With max_pack 1
// every datagram holds one message, which is what the end-to-end benchmark needs to match a
// datagram to its send time.
[[nodiscard]] Workload make_workload(std::size_t book_events, std::uint16_t instruments,
                                     std::uint64_t seed, unsigned max_pack);

// The same for a ground truth made elsewhere, such as a recorded file (Plan 6): its messages,
// with their recorded bytes, packed into datagrams.
[[nodiscard]] Workload make_workload(const sim::GroundTruth& truth, std::uint64_t seed,
                                     unsigned max_pack);

}  // namespace fh::bench

#endif  // FH_BENCH_WORKLOAD_HPP_
