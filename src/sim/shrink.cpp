#include "fh/sim/shrink.hpp"

#include <algorithm>

#include "fh/sim/checker.hpp"

namespace fh::sim {

Trace shrink(const Trace& trace, const std::function<bool(const Trace&)>& still_fails,
             std::size_t max_runs) {
  Trace best = trace;
  std::size_t runs = 0;
  std::size_t parts = 2;

  while (best.entries.size() >= 2 && runs < max_runs) {
    const std::size_t size = best.entries.size();
    parts = std::min(parts, size);
    const std::size_t chunk = (size + parts - 1) / parts;

    bool reduced = false;
    for (std::size_t begin = 0; begin < size && runs < max_runs; begin += chunk) {
      const std::size_t end = std::min(size, begin + chunk);
      Trace candidate = best;
      candidate.entries.erase(candidate.entries.begin() + static_cast<std::ptrdiff_t>(begin),
                              candidate.entries.begin() + static_cast<std::ptrdiff_t>(end));
      ++runs;
      if (still_fails(candidate)) {
        best = std::move(candidate);
        parts = std::max<std::size_t>(parts - 1, 2);
        reduced = true;
        break;
      }
    }
    if (reduced) continue;
    if (parts >= size) break;  // every single entry was tried: 1-minimal
    parts = std::min(size, parts * 2);
  }
  return best;
}

Trace shrink_safety_failure(const Trace& trace, std::size_t max_runs) {
  return shrink(trace, [](const Trace& t) { return !run_trace(t).safe(); }, max_runs);
}

}  // namespace fh::sim
