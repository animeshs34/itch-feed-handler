#include "fh/bench/recovery.hpp"

#include <map>
#include <variant>
#include <vector>

#include "fh/nasdaq/handler.hpp"

namespace fh::bench {

RecoveryResult bench_recovery(sim::Tier tier, std::uint64_t first_seed, std::uint64_t scenarios) {
  RecoveryResult result;
  const sim::ScenarioConfig config = sim::scenario_config(tier);
  result.loop_period = config.timing.snapshot_period * static_cast<Nanos>(config.day.instruments);

  for (std::uint64_t s = 0; s < scenarios; ++s) {
    const sim::Trace trace = sim::make_trace(first_seed + s, config);
    nasdaq::Handler handler(sim::to_core_config(trace.core));
    std::map<InstrumentId, Nanos> recovering_since;
    std::vector<Output> out;

    for (const sim::Entry& entry : trace.entries) {
      out.clear();
      switch (entry.kind) {
        case sim::EntryKind::Main:
          handler.on_packet(entry.t, entry.line, entry.bytes, out);
          break;
        case sim::EntryKind::Snapshot:
          handler.on_snapshot_packet(entry.t, entry.bytes, out);
          break;
        case sim::EntryKind::Tick:
          handler.on_tick(entry.t, out);
          break;
      }
      for (const Output& o : out) {
        const auto* change = std::get_if<InstrumentHealthChange>(&o);
        if (change == nullptr) continue;
        if (change->state == InstrumentHealth::Syncing) {
          recovering_since.emplace(change->inst,
                                   entry.t);  // keeps the first time if already recovering
        } else if (const auto it = recovering_since.find(change->inst);
                   it != recovering_since.end()) {
          result.recovery.record(entry.t - it->second);
          ++result.episodes;
          recovering_since.erase(it);
        }
      }
    }
    result.unrecovered += recovering_since.size();
    ++result.scenarios;
  }
  return result;
}

}  // namespace fh::bench
