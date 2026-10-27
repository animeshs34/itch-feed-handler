#include "fh/sim/faults.hpp"

namespace fh::sim {

FaultConfig tier_config(Tier tier) {
  FaultConfig c;
  switch (tier) {
    case Tier::Clean:
      c.jitter = 100 * kMicrosecond;  // even a clean day has a little reordering
      c.skew_b = 50 * kMicrosecond;
      break;
    case Tier::Mild:
      c.loss_pct = 2;
      c.dup_pct = 2;
      c.jitter = 300 * kMicrosecond;
      c.skew_b = 150 * kMicrosecond;
      c.snap_late_pct = 5;
      c.faulty_pct = 60;
      break;
    case Tier::Moderate:
      c.loss_pct = 6;
      c.burst_pct = 1;
      c.burst_max = 6;
      c.dup_pct = 5;
      c.mismatch_pct = 2;
      c.malformed_pct = 1;
      c.wrong_session_pct = 1;
      c.end_loss_pct = 20;
      c.end_loss_max = 6;
      c.rate_burst_permille = 5;
      c.jitter = 300 * kMicrosecond;
      c.skew_b = 150 * kMicrosecond;
      c.snap_loss_pct = 10;
      c.snap_split_loss_pct = 5;
      c.snap_late_pct = 10;
      c.snap_dup_pct = 5;
      c.faulty_pct = 70;
      break;
    case Tier::Heavy:
      c.loss_pct = 12;
      c.burst_pct = 3;
      c.burst_max = 12;
      c.dup_pct = 8;
      c.mismatch_pct = 4;
      c.malformed_pct = 3;
      c.wrong_session_pct = 3;
      c.silence_windows = 2;
      c.end_loss_pct = 30;
      c.end_loss_max = 10;
      c.rate_burst_permille = 10;
      c.jitter = 600 * kMicrosecond;
      c.skew_b = 300 * kMicrosecond;
      c.snap_loss_pct = 20;
      c.snap_split_loss_pct = 10;
      c.snap_late_pct = 20;
      c.snap_dup_pct = 10;
      c.faulty_pct = 75;
      break;
    case Tier::Brutal:
      c.loss_pct = 25;
      c.burst_pct = 6;
      c.burst_max = 25;
      c.dup_pct = 12;
      c.mismatch_pct = 8;
      c.malformed_pct = 6;
      c.wrong_session_pct = 6;
      c.silence_windows = 4;
      c.end_loss_pct = 40;
      c.end_loss_max = 20;
      c.rate_burst_permille = 20;
      c.jitter = 900 * kMicrosecond;
      c.skew_b = 500 * kMicrosecond;
      c.snap_loss_pct = 35;
      c.snap_split_loss_pct = 20;
      c.snap_late_pct = 30;
      c.snap_dup_pct = 15;
      c.faulty_pct = 80;
      // Small buffers force the overflow paths: reorder overflow, parked overflow.
      c.core.buffer_capacity = 4;
      c.core.arbiter_window = 16;
      c.core.parked_capacity = 4;
      break;
  }
  return c;
}

std::string_view tier_name(Tier tier) {
  switch (tier) {
    case Tier::Clean:
      return "clean";
    case Tier::Mild:
      return "mild";
    case Tier::Moderate:
      return "moderate";
    case Tier::Heavy:
      return "heavy";
    case Tier::Brutal:
      return "brutal";
  }
  return "?";
}

}  // namespace fh::sim
