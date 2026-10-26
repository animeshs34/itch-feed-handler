#ifndef FH_TESTS_SUPPORT_BYTE_TRUTH_HPP_
#define FH_TESTS_SUPPORT_BYTE_TRUTH_HPP_

#include <algorithm>
#include <map>
#include <vector>

#include "fh/events.hpp"
#include "fh/messages.hpp"
#include "fh/nasdaq/itch_encoder.hpp"
#include "fh/nasdaq/mold_encoder.hpp"
#include "oracle_state.hpp"
#include "packets.hpp"

namespace fhtest {

// Ground truth at byte level: message number n (starting at 1) is `messages[n - 1]`,
// and `events` lists the book events with their sequence numbers, which is what the
// oracle needs.
struct ByteTruth {
  std::vector<fh::nasdaq::Bytes> messages;
  std::vector<fh::BookEvent> events;
};

// Turns events into ITCH messages. If `filler_every` is not zero, a message that
// changes no book follows every that-many events, so the sequence numbers contain
// numbers that belong to no book event, as in a real feed.
inline ByteTruth make_byte_truth(const std::vector<fh::BookEvent>& source, unsigned filler_every) {
  ByteTruth truth;
  unsigned counter = 0;
  for (const fh::BookEvent& original : source) {
    fh::BookEvent e = original;
    e.seq = truth.messages.size() + 1;
    fh::nasdaq::EventStyle style;
    style.reduce_as_execute = (e.order % 2 == 0);
    style.match_number = e.seq;
    style.timestamp = e.seq * 1000;
    truth.messages.push_back(fh::nasdaq::encode_event(e, style));
    truth.events.push_back(e);
    if (filler_every != 0 && ++counter % filler_every == 0) {
      const fh::nasdaq::ItchHeader h{e.inst, 0, e.seq * 1000};
      truth.messages.push_back(counter % 2 == 0 ? fh::nasdaq::encode_filler('N', h)
                                                : fh::nasdaq::encode_filler('B', h));
    }
  }
  return truth;
}

// One instrument's snapshot as snapshot-stream messages: R, then A for each order, then G.
inline std::vector<fh::nasdaq::Bytes> snapshot_messages(
    fh::InstrumentId inst, fh::Seq asof, const std::map<fh::OrderId, fh::Order>& orders) {
  using namespace fh::nasdaq;
  std::vector<Bytes> out;
  out.push_back(encode_stock_directory(ItchHeader{inst, 0, 0}, synthetic_stock_name(inst)));
  for (const auto& [id, order] : orders) {
    out.push_back(encode_add(ItchHeader{inst, 0, 0}, id, order.side, order.shares,
                             synthetic_stock_name(inst), order.price));
  }
  out.push_back(encode_end_of_snapshot(asof + 1));
  return out;
}

// What the shell would hand to the handler, at a given time.
struct ByteStep {
  enum class Kind { Main, Snapshot, Tick };
  Kind kind{Kind::Tick};
  fh::Nanos t{0};
  fh::Line line{fh::Line::A};
  fh::nasdaq::Bytes bytes;
};

// Like `Timeline`, but producing bytes: one message per main-feed packet, and one
// complete run per snapshot packet. One unit of time is one millisecond.
class ByteTimeline {
 public:
  static constexpr fh::Nanos kUnit = fh::kMillisecond;

  explicit ByteTimeline(const ByteTruth& truth)
      : truth_(truth),
        main_session_(fh::nasdaq::make_session("MAINSESS01")),
        snapshot_session_(fh::nasdaq::make_session("SNAPSESS01")) {}

  const fh::nasdaq::SessionId& main_session() const { return main_session_; }
  const fh::nasdaq::SessionId& snapshot_session() const { return snapshot_session_; }

  // The packet carrying message `seq`, delivered on `line` at `unit`.
  void deliver(fh::Line line, fh::Nanos unit, fh::Seq seq) {
    const std::vector<fh::nasdaq::Bytes> one = {truth_.messages[seq - 1]};
    add(unit, ByteStep{ByteStep::Kind::Main, unit * kUnit, line,
                       fh::nasdaq::encode_packet(main_session_, seq, one)});
  }

  void deliver_all(fh::Line line, const std::vector<fh::Seq>& seqs, fh::Nanos offset) {
    for (const fh::Seq s : seqs) deliver(line, static_cast<fh::Nanos>(s) + offset, s);
  }

  // Delivers arbitrary bytes (a damaged packet, say).
  void deliver_bytes(fh::Line line, fh::Nanos unit, fh::nasdaq::Bytes bytes) {
    add(unit, ByteStep{ByteStep::Kind::Main, unit * kUnit, line, std::move(bytes)});
  }

  void heartbeat(fh::Line line, fh::Nanos unit, fh::Seq next_seq) {
    add(unit, ByteStep{ByteStep::Kind::Main, unit * kUnit, line,
                       fh::nasdaq::encode_heartbeat(main_session_, next_seq)});
  }

  void end_of_session(fh::Line line, fh::Nanos unit, fh::Seq next_seq) {
    add(unit, ByteStep{ByteStep::Kind::Main, unit * kUnit, line,
                       fh::nasdaq::encode_end_of_session(main_session_, next_seq)});
  }

  // A complete snapshot run for one instrument, built from the oracle, in one packet.
  void snapshot(fh::Nanos unit, fh::InstrumentId inst, fh::Seq asof) {
    const auto messages = snapshot_messages(inst, asof, oracle_orders(truth_.events, inst, asof));
    add(unit, ByteStep{ByteStep::Kind::Snapshot, unit * kUnit, fh::Line::A,
                       fh::nasdaq::encode_packet(snapshot_session_, stream_seq_, messages)});
    stream_seq_ += messages.size();
  }

  // A snapshot-stream packet with a chosen number and chosen messages.
  void snapshot_raw(fh::Nanos unit, fh::Seq stream_seq,
                    const std::vector<fh::nasdaq::Bytes>& messages) {
    add(unit, ByteStep{ByteStep::Kind::Snapshot, unit * kUnit, fh::Line::A,
                       fh::nasdaq::encode_packet(snapshot_session_, stream_seq, messages)});
  }

  // Moves the snapshot stream's numbering on by `count` (for packets sent by hand).
  void advance_stream(std::size_t count) { stream_seq_ += count; }

  // A run that is built (so the numbering advances) but never delivered.
  void snapshot_lost(fh::InstrumentId inst, fh::Seq asof) {
    stream_seq_ += snapshot_messages(inst, asof, oracle_orders(truth_.events, inst, asof)).size();
  }

  std::vector<ByteStep> build(fh::Nanos end_unit) const {
    std::vector<ByteStep> out;
    std::size_t next = 0;
    for (fh::Nanos unit = 1; unit <= end_unit; ++unit) {
      while (next < items_.size() && items_[next].first == unit)
        out.push_back(items_[next++].second);
      out.push_back(ByteStep{ByteStep::Kind::Tick, unit * kUnit, fh::Line::A, {}});
    }
    return out;
  }

 private:
  void add(fh::Nanos unit, ByteStep step) {
    const auto pos = std::upper_bound(items_.begin(), items_.end(), unit,
                                      [](fh::Nanos u, const auto& item) { return u < item.first; });
    items_.insert(pos, {unit, std::move(step)});
  }

  const ByteTruth& truth_;
  fh::nasdaq::SessionId main_session_;
  fh::nasdaq::SessionId snapshot_session_;
  std::vector<std::pair<fh::Nanos, ByteStep>> items_;
  fh::Seq stream_seq_{1};
};

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_BYTE_TRUTH_HPP_
