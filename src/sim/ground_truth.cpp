#include "fh/sim/ground_truth.hpp"

namespace fh::sim {

std::vector<BookEvent> GroundTruth::events() const {
  std::vector<BookEvent> out;
  for (const GroundMessage& m : messages) {
    if (m.event) out.push_back(*m.event);
  }
  return out;
}

std::set<InstrumentId> GroundTruth::instruments() const {
  std::set<InstrumentId> out;
  for (const GroundMessage& m : messages) {
    if (m.event) out.insert(m.event->inst);
  }
  return out;
}

}  // namespace fh::sim
