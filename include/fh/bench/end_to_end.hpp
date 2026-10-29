#ifndef FH_BENCH_END_TO_END_HPP_
#define FH_BENCH_END_TO_END_HPP_

#include <cstdint>
#include <optional>
#include <string>

#include "fh/bench/histogram.hpp"
#include "fh/bench/workload.hpp"
#include "fh/time.hpp"

namespace fh::bench {

struct EndToEndResult {
  LogLinearHistogram latency;  // due time of the send to handler-finished, per datagram
  std::uint64_t sent{0};
  std::uint64_t received{0};
  Nanos wall{0};
  double offered_rate{0};
  [[nodiscard]] double achieved_rate() const noexcept;
};

// The same, through real multicast sockets on the loopback interface: a sender thread
// sends datagram i at its due time; the handler runs behind the wait loop of Plan 7. The
// workload must have one message per datagram, so that a datagram can be matched with its
// send time by its sequence number. Returns nothing, with the reason in `error`, if the
// sockets cannot be opened or the workload does not fit.
[[nodiscard]] std::optional<EndToEndResult> bench_end_to_end(const Workload& workload,
                                                             double datagrams_per_second,
                                                             std::uint64_t count,
                                                             std::uint64_t warmup,
                                                             std::string& error);

}  // namespace fh::bench

#endif  // FH_BENCH_END_TO_END_HPP_
