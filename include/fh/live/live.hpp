#ifndef FH_LIVE_LIVE_HPP_
#define FH_LIVE_LIVE_HPP_

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "fh/net/pump.hpp"
#include "fh/net/udp.hpp"
#include "fh/outputs.hpp"
#include "fh/sim/trace.hpp"

namespace fh::live {

// The three multicast groups of the Nasdaq-style feed: the main feed on two lines, and the
// snapshot stream.
struct Groups {
  net::Endpoint line_a;
  net::Endpoint line_b;
  net::Endpoint snapshots;
};

// The same three groups on consecutive ports starting at `first_port`, all on one address
// and interface. Convenient for the tools and the tests.
[[nodiscard]] Groups groups_on(std::uint16_t first_port,
                               const std::string& group_address = "239.255.42.1",
                               const std::string& interface_address = "127.0.0.1");

struct LiveConfig {
  Groups groups;
  sim::CoreParams core;  // the core settings; they travel with the recorded trace
  Nanos tick_period{200 * kMicrosecond};
  bool collect_outputs{true};  // keep every published output (off for long runs)
};

struct LiveResult {
  // Everything the handler was shown, in order, stamped with the clock: a trace that can
  // be saved and replayed. Its `truth` is empty (a live run has no answer key); a caller
  // that has one can set it before checking the trace.
  sim::Trace trace;
  std::vector<Output> outputs;
  net::PumpStats stats;
  std::optional<FeedState> final_state;  // the books at the end, if every instrument was live
};

// Listens on the three groups, feeds a Nasdaq handler with what arrives, records it all,
// and returns when `stop()` is true. Returns nothing, with the reason in `error`, if a
// group could not be joined.
//
// This is the composition the layers keep apart: sockets (`net`) know nothing of Nasdaq;
// the handler (`nasdaq`) knows nothing of sockets; this joins them.
[[nodiscard]] std::optional<LiveResult> run_live(const LiveConfig& config,
                                                 const std::function<bool()>& stop,
                                                 std::string& error);

// How a trace is sent through real sockets.
struct SendConfig {
  Groups groups;
  // Real time per unit of trace time: an entry stamped `t` is sent `t * scale` after the
  // start. A scale above 1 slows the day down, which makes scheduling jitter small next to
  // the handler's windows (scale those up by the same factor with `scale_core`).
  std::int64_t scale{1};
};

struct SendStats {
  std::uint64_t sent{0};
  std::uint64_t failed{0};  // the kernel refused the datagram
  Nanos worst_lateness{0};  // the longest an entry left after its scheduled time
};

// Sends the main-feed and snapshot entries of `trace` (not the ticks) on their groups, each
// at its time. Returns nothing, with the reason in `error`, if a group could not be opened.
[[nodiscard]] std::optional<SendStats> send_trace(const sim::Trace& trace, const SendConfig& config,
                                                  std::string& error);

// The core settings with every time multiplied by `factor`.
[[nodiscard]] sim::CoreParams scale_core(const sim::CoreParams& params, std::int64_t factor);

}  // namespace fh::live

#endif  // FH_LIVE_LIVE_HPP_
