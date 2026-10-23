#ifndef FH_SNAPSHOT_REASSEMBLER_HPP_
#define FH_SNAPSHOT_REASSEMBLER_HPP_

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "fh/messages.hpp"
#include "fh/order_book.hpp"
#include "fh/types.hpp"

namespace fh {

// One instrument's complete snapshot: every live order as of `asof`.
struct SnapshotRun {
  InstrumentId inst{0};
  Seq asof{0};
  std::map<OrderId, Order> orders;

  friend bool operator==(const SnapshotRun&, const SnapshotRun&) = default;
};

// Puts the pieces of a snapshot back together.
//
// A snapshot arrives as a run of items: Open, then one Order per live order,
// then Close. A run is only trusted if every piece arrived: the snapshot
// stream's own sequence numbers must be unbroken, the pieces must belong to
// one instrument, and the run must be closed. Anything else is thrown away
// and counted, and the next Open starts afresh.
class SnapshotReassembler {
 public:
  // Processes one packet and appends every run it completes to `completed`.
  void accept(const SnapshotPacketIn& packet, std::vector<SnapshotRun>& completed);

  [[nodiscard]] std::uint64_t discarded_runs() const noexcept { return discarded_runs_; }
  [[nodiscard]] bool run_open() const noexcept { return partial_.has_value(); }

 private:
  void discard_partial();

  std::optional<Seq> next_stream_seq_;
  std::optional<SnapshotRun> partial_;
  std::uint64_t discarded_runs_{0};
};

}  // namespace fh

#endif  // FH_SNAPSHOT_REASSEMBLER_HPP_
