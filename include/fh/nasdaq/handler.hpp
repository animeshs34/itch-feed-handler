#ifndef FH_NASDAQ_HANDLER_HPP_
#define FH_NASDAQ_HANDLER_HPP_

#include <cstdint>
#include <span>
#include <vector>

#include "fh/core.hpp"
#include "fh/nasdaq/moldudp64.hpp"
#include "fh/nasdaq/snapshot_decoder.hpp"

namespace fh::nasdaq {

// The whole Nasdaq feed handler: the byte decoders in front of the venue-neutral
// core. Bytes go in; updates, health changes, gaps and alarms come out.
//
// This is where "the codec seam" meets "the core". Everything Nasdaq-specific is
// in the two decoders; the core knows nothing about MoldUDP64 or ITCH.
class Handler {
 public:
  explicit Handler(const fh::CoreConfig& config) : core_(config) {}

  // A datagram from line A or B of the main feed.
  void on_packet(fh::Nanos t, fh::Line line, std::span<const std::uint8_t> bytes,
                 std::vector<fh::Output>& out);

  // A datagram from the snapshot stream.
  void on_snapshot_packet(fh::Nanos t, std::span<const std::uint8_t> bytes,
                          std::vector<fh::Output>& out);

  // Time passing with nothing received.
  void on_tick(fh::Nanos t, std::vector<fh::Output>& out);

  [[nodiscard]] const fh::Core& core() const noexcept { return core_; }
  [[nodiscard]] const DecoderStats& packet_stats() const noexcept { return packets_.stats(); }
  [[nodiscard]] const DecoderStats& snapshot_stats() const noexcept { return snapshots_.stats(); }

 private:
  fh::Core core_;
  PacketDecoder packets_;
  SnapshotStreamDecoder snapshots_;
};

}  // namespace fh::nasdaq

#endif  // FH_NASDAQ_HANDLER_HPP_
