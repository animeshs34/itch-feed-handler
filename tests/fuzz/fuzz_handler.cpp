// libFuzzer target: feeds arbitrary bytes to the Nasdaq handler.
//
// The first input byte chooses where the rest goes: main feed line A, line B, or the
// snapshot stream. A fresh handler is primed with one valid packet first, so the
// fuzzer is exploring "a datagram arriving in a running session", and a new handler
// is made for every input so any crash is reproducible from the input alone.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "fh/nasdaq/handler.hpp"
#include "fh/nasdaq/itch_encoder.hpp"
#include "fh/nasdaq/mold_encoder.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size < 1) return 0;
  using namespace fh::nasdaq;

  Handler handler{fh::CoreConfig{}};
  std::vector<fh::Output> out;

  const SessionId main_session = make_session("MAINSESS01");
  const SessionId snap_session = make_session("SNAPSESS01");
  const std::vector<Bytes> prime = {
      encode_add(ItchHeader{1, 0, 1}, 101, fh::Side::Buy, 100, "I1", 100),
      encode_delete(ItchHeader{1, 0, 2}, 101)};
  handler.on_packet(1, fh::Line::A, encode_packet(main_session, 1, prime), out);
  handler.on_snapshot_packet(
      2,
      encode_packet(snap_session, 1,
                    std::vector<Bytes>{encode_stock_directory(ItchHeader{1, 0, 0}, "I1"),
                                       encode_end_of_snapshot(3)}),
      out);

  const std::span<const std::uint8_t> datagram(data + 1, size - 1);
  switch (data[0] % 3) {
    case 0:
      handler.on_packet(3, fh::Line::A, datagram, out);
      break;
    case 1:
      handler.on_packet(3, fh::Line::B, datagram, out);
      break;
    default:
      handler.on_snapshot_packet(3, datagram, out);
      break;
  }
  handler.on_tick(10 * fh::kSecond, out);
  return 0;
}
