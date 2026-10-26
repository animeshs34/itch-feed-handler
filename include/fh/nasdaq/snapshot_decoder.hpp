#ifndef FH_NASDAQ_SNAPSHOT_DECODER_HPP_
#define FH_NASDAQ_SNAPSHOT_DECODER_HPP_

#include <cstdint>
#include <optional>
#include <span>

#include "fh/messages.hpp"
#include "fh/nasdaq/moldudp64.hpp"

namespace fh::nasdaq {

// Turns datagrams of the snapshot stream into decoded snapshot packets.
//
// The snapshot stream is its own MoldUDP64 stream with its own session and its
// own sequence numbers (Appendix B.5). One instrument's snapshot is the run of
// messages
//
//     R  stock directory         ->  SnapshotOpen   (the instrument's locate)
//     A  add order (or F)        ->  SnapshotOrder  (one per live order)
//     G  end of snapshot         ->  SnapshotClose  (as-of = the number in G, minus 1)
//
// Every message becomes exactly one item, so the snapshot stream's numbering
// matches the item count and the reassembler can notice a lost packet. A packet
// containing anything else is dropped whole.
class SnapshotStreamDecoder {
 public:
  [[nodiscard]] std::optional<fh::SnapshotPacketIn> decode(fh::Nanos t,
                                                           std::span<const std::uint8_t> bytes);

  [[nodiscard]] const DecoderStats& stats() const noexcept { return stats_; }

 private:
  std::optional<SessionId> session_;
  DecoderStats stats_;
};

// Reads the 20 character number of an end-of-snapshot message (spaces, then digits).
// Returns nullopt if it is not a number that fits in 64 bits.
[[nodiscard]] std::optional<std::uint64_t> parse_resume_seq(std::span<const std::uint8_t> field);

}  // namespace fh::nasdaq

#endif  // FH_NASDAQ_SNAPSHOT_DECODER_HPP_
