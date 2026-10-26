#include "fh/nasdaq/mold_encoder.hpp"

#include <algorithm>
#include <cassert>

namespace fh::nasdaq {

SessionId make_session(std::string_view name) {
  SessionId id;
  id.fill(' ');
  std::copy_n(name.begin(), std::min(name.size(), id.size()), id.begin());
  return id;
}

namespace {

void put_be(Bytes& out, std::uint64_t value, std::size_t width) {
  for (std::size_t i = width; i > 0; --i) {
    out.push_back(static_cast<std::uint8_t>(value >> (8 * (i - 1))));
  }
}

Bytes header(const SessionId& session, fh::Seq seq, std::uint16_t count) {
  Bytes out;
  out.reserve(kMoldHeaderBytes);
  for (const char c : session) out.push_back(static_cast<std::uint8_t>(c));
  put_be(out, seq, 8);
  put_be(out, count, 2);
  return out;
}

}  // namespace

Bytes encode_packet(const SessionId& session, fh::Seq first_seq, std::span<const Bytes> messages) {
  assert(messages.size() < kMoldEndOfSessionCount);
  Bytes out = header(session, first_seq, static_cast<std::uint16_t>(messages.size()));
  for (const Bytes& message : messages) {
    assert(message.size() <= 0xFFFF);
    put_be(out, message.size(), 2);
    out.insert(out.end(), message.begin(), message.end());
  }
  return out;
}

Bytes encode_heartbeat(const SessionId& session, fh::Seq next_seq) {
  return header(session, next_seq, kMoldHeartbeatCount);
}

Bytes encode_end_of_session(const SessionId& session, fh::Seq next_seq) {
  return header(session, next_seq, kMoldEndOfSessionCount);
}

}  // namespace fh::nasdaq
