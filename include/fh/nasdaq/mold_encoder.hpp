#ifndef FH_NASDAQ_MOLD_ENCODER_HPP_
#define FH_NASDAQ_MOLD_ENCODER_HPP_

#include <span>

#include "fh/nasdaq/itch_encoder.hpp"
#include "fh/nasdaq/moldudp64.hpp"

namespace fh::nasdaq {

// Builds MoldUDP64 packets: the mirror image of the packet decoder, used by tests
// and by the simulator.

// A packet carrying `messages`, numbered from `first_seq`.
[[nodiscard]] Bytes encode_packet(const SessionId& session, fh::Seq first_seq,
                                  std::span<const Bytes> messages);

// A heartbeat saying the next message will be `next_seq`.
[[nodiscard]] Bytes encode_heartbeat(const SessionId& session, fh::Seq next_seq);

// An end-of-session packet saying the next message would have been `next_seq`.
[[nodiscard]] Bytes encode_end_of_session(const SessionId& session, fh::Seq next_seq);

}  // namespace fh::nasdaq

#endif  // FH_NASDAQ_MOLD_ENCODER_HPP_
