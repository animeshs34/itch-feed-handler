#include "fh/sim/snapshots.hpp"

#include <algorithm>
#include <cassert>
#include <span>

#include "fh/nasdaq/mold_encoder.hpp"

namespace fh::sim {

SnapshotBuilder::SnapshotBuilder(const GroundTruth& truth) : timeline_(truth.events()) {}

std::vector<SnapshotDatagram> SnapshotBuilder::build_run(Nanos sent, InstrumentId inst, Seq asof,
                                                         std::size_t max_messages) {
  assert(max_messages >= 3);
  using namespace nasdaq;
  const auto orders = timeline_.orders_at(inst, asof);

  std::vector<Bytes> messages;
  messages.push_back(encode_stock_directory(ItchHeader{inst, 0, 0}, synthetic_stock_name(inst)));
  for (const auto& [id, order] : orders) {
    messages.push_back(encode_add(ItchHeader{inst, 0, 0}, id, order.side, order.shares,
                                  synthetic_stock_name(inst), order.price));
  }
  messages.push_back(encode_end_of_snapshot(asof + 1));

  const std::size_t parts = (messages.size() + max_messages - 1) / max_messages;
  std::vector<SnapshotDatagram> out;
  for (std::size_t p = 0; p < parts; ++p) {
    const std::size_t begin = p * max_messages;
    const std::size_t end = std::min(messages.size(), begin + max_messages);
    SnapshotDatagram d;
    d.sent = sent;
    d.inst = inst;
    d.asof = asof;
    d.run = runs_;
    d.index = p;
    d.of = parts;
    d.first_stream_seq = stream_seq_ + begin;
    d.count = static_cast<std::uint16_t>(end - begin);
    d.bytes = encode_packet(snapshot_session(), d.first_stream_seq,
                            std::span<const Bytes>(messages).subspan(begin, end - begin));
    out.push_back(std::move(d));
  }
  stream_seq_ += messages.size();
  ++runs_;
  return out;
}

}  // namespace fh::sim
