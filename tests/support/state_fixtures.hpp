#ifndef FH_TESTS_SUPPORT_STATE_FIXTURES_HPP_
#define FH_TESTS_SUPPORT_STATE_FIXTURES_HPP_

#include <catch2/catch_test_macros.hpp>

#include "fh/state_codec.hpp"

namespace fhtest {

// The smallest interesting state: sequence 7, instrument 1, one buy order
// (id 101, price 100, 10 shares). Used for byte-exact checks.
inline fh::FeedState one_order_state() {
  fh::FeedState s;
  s.seq = 7;
  REQUIRE(s.books[1].apply(fh::make_add(7, 1, 101, fh::Side::Buy, 100, 10)).ok());
  return s;
}

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_STATE_FIXTURES_HPP_
