#ifndef FH_INSTRUMENT_STATE_HPP_
#define FH_INSTRUMENT_STATE_HPP_

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "fh/events.hpp"
#include "fh/health.hpp"
#include "fh/order_book.hpp"
#include "fh/outputs.hpp"
#include "fh/snapshot_reassembler.hpp"
#include "fh/types.hpp"

namespace fh {

struct InstrumentConfig {
  // How many messages may wait for a snapshot while an instrument is recovering.
  std::size_t parked_capacity{65536};
};

enum class EventOutcome : std::uint8_t {
  Applied,         // applied to a live book and published
  Covered,         // already inside a snapshot we loaded: skipped
  Parked,          // the instrument is recovering: kept for after the snapshot
  Inconsistent,    // the book could not apply it: the instrument went back to recovering
  ParkedOverflow,  // too many parked: they were dropped and a later snapshot is required
};

enum class SnapshotOutcome : std::uint8_t {
  Ignored,   // the instrument is already live
  Stale,     // older than what is needed to repair the gap
  Rejected,  // the orders do not form a valid book
  Applied,   // loaded; the instrument is live again
};

// One instrument: its book, whether the book can be trusted, and everything
// needed to repair it.
//
//   need      the snapshot that repairs this instrument must be as-of at least this
//   last_seq  the sequence number of the last event reflected in the book
//   pending   events that arrived while recovering, kept until a snapshot lands
//   epoch     counts the snapshots loaded
class InstrumentState {
 public:
  // `need == 0` means nothing is missing, so the instrument starts live with an
  // empty book. Otherwise it starts recovering.
  InstrumentState(InstrumentId id, Seq need, const InstrumentConfig& config);

  EventOutcome on_event(const BookEvent& event, std::vector<Output>& out);

  // Sequence numbers up to `to` were lost: whatever the book held may be wrong.
  void on_gap(Seq to, std::vector<Output>& out);

  SnapshotOutcome on_snapshot(const SnapshotRun& run, std::vector<Output>& out);

  [[nodiscard]] InstrumentId id() const noexcept { return id_; }
  [[nodiscard]] InstrumentHealth health() const noexcept { return health_; }
  [[nodiscard]] Seq need() const noexcept { return need_; }
  [[nodiscard]] Seq last_seq() const noexcept { return last_seq_; }
  [[nodiscard]] std::uint32_t epoch() const noexcept { return epoch_; }
  [[nodiscard]] const InstrumentBook& book() const noexcept { return book_; }
  [[nodiscard]] std::size_t pending() const noexcept { return pending_.size(); }
  [[nodiscard]] std::uint64_t inconsistencies() const noexcept { return inconsistencies_; }

 private:
  EventOutcome apply_live(const BookEvent& event, std::vector<Output>& out);

  InstrumentId id_;
  InstrumentConfig config_;
  InstrumentHealth health_;
  Seq need_;
  Seq last_seq_{0};
  std::uint32_t epoch_{0};
  InstrumentBook book_;
  std::map<Seq, BookEvent> pending_;
  std::uint64_t inconsistencies_{0};
};

}  // namespace fh

#endif  // FH_INSTRUMENT_STATE_HPP_
