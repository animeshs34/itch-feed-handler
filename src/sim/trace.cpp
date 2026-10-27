#include "fh/sim/trace.hpp"

namespace fh::sim {

CoreConfig to_core_config(const CoreParams& params) {
  CoreConfig config;
  config.sequencer.skew_window = params.skew_window;
  config.sequencer.buffer_capacity = params.buffer_capacity;
  config.arbiter_window = params.arbiter_window;
  config.lines.silent_window = params.silent_window;
  config.instruments.parked_capacity = params.parked_capacity;
  return config;
}

}  // namespace fh::sim
