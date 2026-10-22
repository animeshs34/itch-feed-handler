#ifndef FH_TESTS_SUPPORT_ORACLE_STATE_HPP_
#define FH_TESTS_SUPPORT_ORACLE_STATE_HPP_

#include <map>
#include <set>
#include <vector>

#include "fh/state_codec.hpp"
#include "oracle_book.hpp"

namespace fhtest {

// The set of instruments that appear anywhere in `events`.
inline std::set<fh::InstrumentId> instruments_of(const std::vector<fh::BookEvent>& events) {
  std::set<fh::InstrumentId> out;
  for (const fh::BookEvent& e : events) out.insert(e.inst);
  return out;
}

// The orders of one instrument after the first `upto` events (by sequence number),
// worked out by the oracle.
inline std::map<fh::OrderId, fh::Order> oracle_orders(const std::vector<fh::BookEvent>& events,
                                                      fh::InstrumentId inst, fh::Seq upto) {
  oracle::Book book;
  for (const fh::BookEvent& e : events) {
    if (e.seq > upto) break;
    if (e.inst == inst) (void)book.apply(e);
  }
  std::map<fh::OrderId, fh::Order> out;
  for (const oracle::Order& o : book.orders_of(inst)) {
    out.emplace(o.id, fh::Order{o.side, o.price, o.shares});
  }
  return out;
}

// Answers "what are this instrument's orders after sequence number N?" many times
// without replaying the whole history for each question. It keeps one oracle book
// per instrument and moves it forward; a question about an earlier time starts that
// instrument's book again from the beginning.
class OracleTimeline {
 public:
  explicit OracleTimeline(const std::vector<fh::BookEvent>& events) {
    for (const fh::BookEvent& e : events) by_instrument_[e.inst].events.push_back(e);
  }

  std::map<fh::OrderId, fh::Order> orders_at(fh::InstrumentId inst, fh::Seq seq) {
    Cursor& cursor = by_instrument_[inst];
    if (seq < cursor.at) {
      cursor.book = oracle::Book{};
      cursor.next = 0;
    }
    while (cursor.next < cursor.events.size() && cursor.events[cursor.next].seq <= seq) {
      (void)cursor.book.apply(cursor.events[cursor.next]);
      ++cursor.next;
    }
    cursor.at = seq;
    std::map<fh::OrderId, fh::Order> out;
    for (const oracle::Order& o : cursor.book.orders_of(inst)) {
      out.emplace(o.id, fh::Order{o.side, o.price, o.shares});
    }
    return out;
  }

 private:
  struct Cursor {
    std::vector<fh::BookEvent> events;
    oracle::Book book;
    std::size_t next{0};
    fh::Seq at{0};
  };
  std::map<fh::InstrumentId, Cursor> by_instrument_;
};

// The whole feed state after `upto`, built from the oracle's answer.
inline fh::FeedState oracle_state(const std::vector<fh::BookEvent>& events, fh::Seq upto) {
  fh::FeedState state;
  state.seq = upto;
  for (const fh::InstrumentId inst : instruments_of(events)) {
    auto book = fh::InstrumentBook::from_orders(oracle_orders(events, inst, upto));
    state.books.emplace(inst, std::move(*book));
  }
  return state;
}

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_ORACLE_STATE_HPP_
