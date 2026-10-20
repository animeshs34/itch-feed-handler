#ifndef FH_TESTS_SUPPORT_WALKTHROUGH_HPP_
#define FH_TESTS_SUPPORT_WALKTHROUGH_HPP_

#include <vector>

#include "fh/events.hpp"

namespace fhtest {

// The ten-message example from the walkthrough page. Instruments: 1 = X, 2 = Y.
inline std::vector<fh::BookEvent> walkthrough_events() {
  using fh::Side;
  return {
      fh::make_add(1, 1, 101, Side::Buy, 100, 10),
      fh::make_add(2, 2, 201, Side::Sell, 50, 7),
      fh::make_add(3, 1, 102, Side::Sell, 101, 5),
      fh::make_add(4, 2, 202, Side::Sell, 50, 4),
      fh::make_reduce(5, 1, 101, 4),
      fh::make_replace(6, 1, 102, 103, 102, 8),
      fh::make_reduce(7, 2, 201, 3),
      fh::make_add(8, 1, 104, Side::Buy, 100, 6),
      fh::make_delete(9, 2, 202),
      fh::make_reduce(10, 1, 103, 8),
  };
}

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_WALKTHROUGH_HPP_
