#include "fh/sim/metamorphic.hpp"

#include "fh/sim/checker.hpp"
#include "prng.hpp"

namespace fh::sim {

Trace with_duplicates(const Trace& trace, std::uint64_t seed, unsigned pct) {
  fhtest::SplitMix64 rng(seed);
  Trace out = trace;
  out.entries.clear();
  out.entries.reserve(trace.entries.size() + trace.entries.size() / 4);
  for (const Entry& e : trace.entries) {
    out.entries.push_back(e);
    if (e.kind == EntryKind::Main && rng.below(100) < pct) out.entries.push_back(e);
  }
  return out;
}

std::optional<std::string> check_duplicate_invariance(const Trace& trace, std::uint64_t seed) {
  const Verdict plain = run_trace(trace);
  const Verdict doubled = run_trace(with_duplicates(trace, seed, 30));
  // A duplicated copy of a damaged datagram is one more mismatch, and the handler is right
  // to say so, so line-mismatch alarms are left out of the comparison. Everything else
  // published (updates, health, gaps, other alarms) must be identical.
  if (plain.book_digest != doubled.book_digest) {
    return "P7: extra duplicate datagrams changed what the handler published";
  }
  if (plain.final_checksum != doubled.final_checksum) {
    return "P7: extra duplicate datagrams changed the final books";
  }
  if (doubled.counters.duplicates < plain.counters.duplicates) {
    return "P7: extra duplicate datagrams were not counted as duplicates";
  }
  return std::nullopt;
}

std::optional<std::string> check_determinism(const Trace& trace) {
  const Verdict a = run_trace(trace);
  const Verdict b = run_trace(trace);
  if (a.output_digest != b.output_digest) return "P5: two runs published different outputs";
  if (a.final_checksum != b.final_checksum) return "P5: two runs ended with different books";
  if (!(a.counters == b.counters)) return "P5: two runs counted different things";
  return std::nullopt;
}

}  // namespace fh::sim
