#include "fh/sim/consistency.hpp"

#include <unordered_map>

namespace fh::sim {
namespace {

// The rules of a valid stream, on nothing but a map of live orders to their shares.
// A day of millions of events needs a lookup that does not scan, so this is not the
// oracle's flat list; the two are checked against each other in the tests.
class LiveOrders {
 public:
  bool apply(const BookEvent& e) {
    switch (e.kind) {
      case EventKind::Add:
        if (e.shares == 0 || shares_.contains(e.order)) return false;
        shares_.emplace(e.order, e.shares);
        return true;
      case EventKind::Reduce: {
        const auto it = shares_.find(e.order);
        if (e.shares == 0 || it == shares_.end() || e.shares > it->second) return false;
        it->second -= e.shares;
        if (it->second == 0) shares_.erase(it);
        return true;
      }
      case EventKind::Delete:
        return shares_.erase(e.order) == 1;
      case EventKind::Replace: {
        const auto it = shares_.find(e.order);
        if (e.shares == 0 || it == shares_.end() || shares_.contains(e.new_order)) return false;
        shares_.erase(it);
        shares_.emplace(e.new_order, e.shares);
        return true;
      }
    }
    return false;
  }

 private:
  std::unordered_map<OrderId, Shares> shares_;
};

}  // namespace

ConsistencyReport check_consistency(const GroundTruth& truth) {
  ConsistencyReport report;
  LiveOrders live;
  for (std::size_t i = 0; i < truth.messages.size(); ++i) {
    const GroundMessage& m = truth.messages[i];
    if (m.seq != i + 1) report.numbering_ok = false;
    if (!m.event) continue;

    ++report.events_checked;
    if (!live.apply(*m.event)) {
      ++report.violations;
      if (report.first.size() < kMaxListedViolations) {
        report.first.push_back(Violation{m.seq, m.event->kind, m.event->inst, m.event->order});
      }
    }
  }
  return report;
}

}  // namespace fh::sim
