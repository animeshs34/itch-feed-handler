#include "fh/nasdaq/handler.hpp"

#include <utility>

namespace fh::nasdaq {

void Handler::on_packet(fh::Nanos t, fh::Line line, std::span<const std::uint8_t> bytes,
                        std::vector<fh::Output>& out) {
  if (auto packet = packets_.decode(t, line, bytes)) {
    core_.step(fh::Input{std::move(*packet)}, out);
  }
  // A packet that could not be decoded is dropped here; the decoder has counted it.
}

void Handler::on_snapshot_packet(fh::Nanos t, std::span<const std::uint8_t> bytes,
                                 std::vector<fh::Output>& out) {
  if (auto packet = snapshots_.decode(t, bytes)) {
    core_.step(fh::Input{std::move(*packet)}, out);
  }
}

void Handler::on_tick(fh::Nanos t, std::vector<fh::Output>& out) {
  core_.step(fh::Input{fh::TickIn{t}}, out);
}

}  // namespace fh::nasdaq
