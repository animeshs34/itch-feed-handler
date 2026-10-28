#ifndef FH_NET_CLOCK_HPP_
#define FH_NET_CLOCK_HPP_

#include <chrono>

#include "fh/time.hpp"

namespace fh::net {

// The one place the handler's world reads a clock. It counts nanoseconds from the moment
// the clock was made, on the monotonic clock (`std::chrono::steady_clock`), which never
// goes backwards and does not jump when someone sets the time of day. The core never calls
// this; the shell does, and passes the number in (Plan 2, C22).
class MonotonicClock {
 public:
  MonotonicClock() noexcept : start_(std::chrono::steady_clock::now()) {}

  [[nodiscard]] Nanos now() const noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() -
                                                                start_)
        .count();
  }

 private:
  std::chrono::steady_clock::time_point start_;
};

}  // namespace fh::net

#endif  // FH_NET_CLOCK_HPP_
