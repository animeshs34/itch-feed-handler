#include "fh/sim/coverage.hpp"

#include <algorithm>
#include <cstdio>

namespace fh::sim {
namespace {

struct Probe {
  const char* name;
  Expect expect;
  std::uint64_t value;
};

// The value of every probe for one scenario. The order is the order of the table.
std::vector<Probe> probes_of(const Verdict& v) {
  const auto packet_error = [&v](nasdaq::PacketError e) {
    return v.packet_stats.packet_errors[static_cast<std::size_t>(e)];
  };
  return {
      {"updates published", Expect::Reached, v.updates},
      {"book resets (snapshot loaded)", Expect::Reached, v.resets},
      {"gap: timeout", Expect::Reached,
       v.gap_reasons[static_cast<std::size_t>(GapReason::Timeout)]},
      {"gap: tail (heartbeat showed loss)", Expect::Reached,
       v.gap_reasons[static_cast<std::size_t>(GapReason::Tail)]},
      {"gap: reorder buffer overflow", Expect::Reached,
       v.gap_reasons[static_cast<std::size_t>(GapReason::Overflow)]},
      {"identical duplicates", Expect::Reached, v.counters.duplicates},
      {"copies too old to compare", Expect::Info, v.counters.duplicates_unverified},
      {"line mismatches", Expect::Reached, v.counters.line_mismatches},
      {"late after gap", Expect::Reached, v.counters.late_after_gap},
      {"events covered by a snapshot", Expect::Reached, v.counters.covered},
      {"stale snapshots", Expect::Reached, v.counters.stale_snapshots},
      {"snapshots ignored (instrument live)", Expect::Reached, v.counters.ignored_snapshots},
      {"parked overflows", Expect::Reached, v.counters.parked_overflows},
      {"feed health: degraded", Expect::Reached,
       v.feed_health[static_cast<std::size_t>(FeedHealth::Degraded)]},
      {"feed health: stale", Expect::Reached,
       v.feed_health[static_cast<std::size_t>(FeedHealth::Stale)]},
      {"feed health: ended", Expect::Reached,
       v.feed_health[static_cast<std::size_t>(FeedHealth::Ended)]},
      {"packets refused: structure broken", Expect::Reached,
       packet_error(nasdaq::PacketError::BlockOverrun) +
           packet_error(nasdaq::PacketError::TooShort) +
           packet_error(nasdaq::PacketError::TrailingBytes)},
      {"packets refused: wrong session", Expect::Reached,
       packet_error(nasdaq::PacketError::SessionMismatch)},
      {"alarm: line mismatch", Expect::Reached,
       v.alarms[static_cast<std::size_t>(AlarmKind::LineMismatch)]},
      {"alarm: book inconsistent (P9)", Expect::Zero,
       v.alarms[static_cast<std::size_t>(AlarmKind::BookInconsistent)]},
      {"alarm: snapshot rejected", Expect::Zero,
       v.alarms[static_cast<std::size_t>(AlarmKind::SnapshotRejected)]},
      {"inconsistencies counted", Expect::Zero, v.counters.inconsistencies},
      {"safety failures (P1 P2 P3 P9)", Expect::Zero, v.safety_failures.size()},
      {"liveness failures (P4)", Expect::Zero, v.liveness_failures.size()},
  };
}

}  // namespace

Coverage::Coverage() {
  for (const Probe& p : probes_of(Verdict{})) rows_.push_back(CoverageRow{p.name, p.expect, 0, 0});
}

void Coverage::add(const Verdict& verdict) {
  ++scenarios_;
  const std::vector<Probe> probes = probes_of(verdict);
  for (std::size_t i = 0; i < probes.size(); ++i) {
    rows_[i].total += probes[i].value;
    rows_[i].scenarios += probes[i].value > 0 ? 1U : 0U;
  }
}

std::vector<std::string> Coverage::unreached() const {
  std::vector<std::string> out;
  for (const CoverageRow& r : rows_) {
    if (r.expect == Expect::Reached && r.scenarios == 0) out.push_back(r.name);
  }
  return out;
}

std::vector<std::string> Coverage::violated() const {
  std::vector<std::string> out;
  for (const CoverageRow& r : rows_) {
    if (r.expect == Expect::Zero && r.total != 0) out.push_back(r.name);
  }
  return out;
}

std::string Coverage::render() const {
  std::string out;
  char line[160];
  std::snprintf(line, sizeof line, "%-40s %-8s %12s %14s\n", "probe", "expect", "scenarios",
                "total");
  out += line;
  for (const CoverageRow& r : rows_) {
    const char* expect = r.expect == Expect::Reached ? "reached"
                         : r.expect == Expect::Zero  ? "zero"
                                                     : "info";
    std::snprintf(line, sizeof line, "%-40s %-8s %12llu %14llu\n", r.name.c_str(), expect,
                  static_cast<unsigned long long>(r.scenarios),
                  static_cast<unsigned long long>(r.total));
    out += line;
  }
  return out;
}

}  // namespace fh::sim
