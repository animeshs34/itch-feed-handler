#ifndef FH_SIM_SNAPSHOTS_HPP_
#define FH_SIM_SNAPSHOTS_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "fh/sim/ground_truth.hpp"
#include "fh/sim/wire.hpp"
#include "oracle_state.hpp"

namespace fh::sim {

// One datagram of the snapshot stream, before any fault. A run for one instrument
// is a stock directory message, one add message per live order, and an
// end-of-snapshot message; a long run is cut into several datagrams.
struct SnapshotDatagram {
  Nanos sent{0};
  InstrumentId inst{0};
  Seq asof{0};              // the main-feed sequence number the run reflects
  std::size_t run{0};       // which run of the day this belongs to, from 0
  std::size_t index{0};     // position inside the run, from 0
  std::size_t of{0};        // how many datagrams the run has
  Seq first_stream_seq{0};  // snapshot-stream number of the first message inside
  std::uint16_t count{0};   // messages inside
  nasdaq::Bytes bytes;
};

// Builds the snapshot runs. It asks the oracle what an instrument's orders were at
// a sequence number, so the snapshot is right by construction; the snapshot stream's
// own numbering runs on from run to run.
class SnapshotBuilder {
 public:
  explicit SnapshotBuilder(const GroundTruth& truth);

  // The run for `inst` as of `asof`, cut into datagrams of at most `max_messages`
  // messages each (at least 3 so that a run is never cut inside its framing). Every
  // datagram is stamped `sent`.
  [[nodiscard]] std::vector<SnapshotDatagram> build_run(Nanos sent, InstrumentId inst, Seq asof,
                                                        std::size_t max_messages);

  // The stream number the next run will start at.
  [[nodiscard]] Seq next_stream_seq() const noexcept { return stream_seq_; }

 private:
  fhtest::OracleTimeline timeline_;
  Seq stream_seq_{1};
  std::size_t runs_{0};
};

}  // namespace fh::sim

#endif  // FH_SIM_SNAPSHOTS_HPP_
