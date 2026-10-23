#ifndef FH_TESTS_SUPPORT_CONSUMER_MODEL_HPP_
#define FH_TESTS_SUPPORT_CONSUMER_MODEL_HPP_

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "fh/outputs.hpp"
#include "oracle_state.hpp"

namespace fhtest {

// Plays the part of someone who reads the core's published output and trusts
// nothing: it rebuilds each instrument's book from the updates alone and checks
// every update against the oracle and against the continuity rule.
//
// It checks, per the spec:
//   P1  after every update the rebuilt orders equal the oracle's at that sequence number,
//       and the published level totals equal the rebuilt book's levels
//   P2  each Delta's `prev` equals the previous sequence number seen for that instrument
//   P3  no Delta is published while the instrument is recovering
//   P9  no BookInconsistent alarm (valid ground truth must never trigger one)
class ConsumerModel {
 public:
  explicit ConsumerModel(const std::vector<fh::BookEvent>& truth) : timeline_(truth) {}

  void consume(const fh::Output& output) {
    if (const auto* update = std::get_if<fh::Update>(&output)) {
      on_update(*update);
    } else if (const auto* health = std::get_if<fh::InstrumentHealthChange>(&output)) {
      syncing_[health->inst] = health->state == fh::InstrumentHealth::Syncing;
    } else if (const auto* alarm = std::get_if<fh::Alarm>(&output)) {
      if (alarm->kind == fh::AlarmKind::BookInconsistent) {
        fail("P9: BookInconsistent alarm for instrument " + std::to_string(alarm->inst) +
             " at seq " + std::to_string(alarm->seq));
      }
    }
  }

  [[nodiscard]] bool ok() const noexcept { return failures_.empty(); }
  [[nodiscard]] const std::vector<std::string>& failures() const noexcept { return failures_; }
  [[nodiscard]] std::size_t updates_seen() const noexcept { return updates_seen_; }

 private:
  struct Track {
    fh::InstrumentBook book;
    fh::Seq last{0};
  };

  void fail(std::string message) {
    if (failures_.size() < 10) failures_.push_back(std::move(message));
  }

  void on_update(const fh::Update& u) {
    ++updates_seen_;
    Track& track = tracks_[u.inst];
    const std::string where =
        "instrument " + std::to_string(u.inst) + " seq " + std::to_string(u.seq) + ": ";

    if (u.kind == fh::UpdateKind::Reset) {
      auto book = fh::InstrumentBook::from_orders(u.orders);
      if (!book) {
        fail(where + "Reset carries an invalid order list");
        return;
      }
      track.book = std::move(*book);
      track.last = u.seq;
      syncing_[u.inst] = false;
    } else {
      if (syncing_[u.inst]) fail("P3: " + where + "Delta published while recovering");
      if (!u.prev.has_value() || *u.prev != track.last) {
        fail("P2: " + where + "prev does not match the last seq seen (" +
             std::to_string(track.last) + ")");
      }
      if (!track.book.apply(u.event).ok()) {
        fail(where + "the event cannot be applied to the rebuilt book");
        return;
      }
      track.last = u.seq;
      // The published level totals must agree with the rebuilt book.
      for (std::uint8_t i = 0; i < u.level_count; ++i) {
        if (track.book.levels().total(u.levels[i].key) != u.levels[i].total_after) {
          fail("P1b: " + where + "published level total differs from the rebuilt book");
        }
      }
    }

    if (track.book.orders() != timeline_.orders_at(u.inst, u.seq)) {
      fail("P1: " + where + "rebuilt orders differ from the oracle");
    }
  }

  OracleTimeline timeline_;
  std::map<fh::InstrumentId, Track> tracks_;
  std::map<fh::InstrumentId, bool> syncing_;
  std::vector<std::string> failures_;
  std::size_t updates_seen_{0};
};

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_CONSUMER_MODEL_HPP_
