#ifndef FH_SIM_WIRE_HPP_
#define FH_SIM_WIRE_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "fh/messages.hpp"
#include "fh/nasdaq/itch_encoder.hpp"
#include "fh/nasdaq/moldudp64.hpp"
#include "fh/sim/ground_truth.hpp"
#include "fh/time.hpp"
#include "prng.hpp"

namespace fh::sim {

// The two MoldUDP64 sessions of a simulated day: one for the main feed (both lines
// carry it) and one for the snapshot stream.
[[nodiscard]] nasdaq::SessionId main_session();
[[nodiscard]] nasdaq::SessionId snapshot_session();

// The ITCH bytes of one true message: its recorded bytes if it has them, otherwise
// written from its fields. A Reduce is written as an execution when its order id is
// even and as a cancel when odd, so both wire forms are exercised.
[[nodiscard]] nasdaq::Bytes message_bytes(const GroundMessage& message);

enum class DatagramKind : std::uint8_t { Messages, Heartbeat, EndOfSession };

// One main-feed datagram as it leaves the publisher, before any fault.
struct Datagram {
  Nanos sent{0};
  Line line{Line::A};
  DatagramKind kind{DatagramKind::Messages};
  Seq first_seq{0};        // Messages: the first sequence number inside. Others: the next one.
  std::uint16_t count{0};  // Messages: how many messages are inside
  nasdaq::Bytes bytes;
};

// When each message is sent: `sent[n - 1]` is the time of message n.
// Message n goes out at n * spacing, except inside a rate burst, where messages go out
// one microsecond apart. The result never decreases.
[[nodiscard]] std::vector<Nanos> send_times(std::size_t messages, Nanos spacing,
                                            fhtest::SplitMix64& rng, unsigned burst_pct);

// Packs the true messages into datagrams for one line. Each datagram holds between 1
// and `max_pack` messages, chosen at random, so the two lines cut the feed at
// different places. A datagram leaves at the time its LAST message is sent.
[[nodiscard]] std::vector<Datagram> packetize(const GroundTruth& truth,
                                              const std::vector<Nanos>& sent, Line line,
                                              fhtest::SplitMix64& rng, unsigned max_pack);

// A heartbeat saying the publisher will send `next_seq` next.
[[nodiscard]] Datagram make_heartbeat(Nanos sent, Line line, Seq next_seq);

// An end-of-session packet saying the next message would have been `next_seq`.
[[nodiscard]] Datagram make_end_of_session(Nanos sent, Line line, Seq next_seq);

}  // namespace fh::sim

#endif  // FH_SIM_WIRE_HPP_
