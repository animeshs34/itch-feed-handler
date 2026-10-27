#include "fh/sim/checker.hpp"

#include <variant>

#include "consumer_model.hpp"
#include "fh/checksum.hpp"
#include "fh/nasdaq/handler.hpp"
#include "oracle_state.hpp"

namespace fh::sim {
namespace {

// A running FNV-1a over 64-bit values, used to fingerprint the stream of outputs.
class Fingerprint {
 public:
  void put(std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
      hash_ ^= (value >> (8 * i)) & 0xFF;
      hash_ *= 1099511628211ULL;
    }
  }
  [[nodiscard]] std::uint64_t value() const noexcept { return hash_; }

 private:
  std::uint64_t hash_{14695981039346656037ULL};
};

void put_event(Fingerprint& f, const BookEvent& e) {
  f.put(static_cast<std::uint64_t>(e.kind));
  f.put(e.seq);
  f.put(e.inst);
  f.put(e.order);
  f.put(e.new_order);
  f.put(static_cast<std::uint64_t>(e.side));
  f.put(static_cast<std::uint64_t>(e.price));
  f.put(e.shares);
}

void put_output(Fingerprint& f, const Output& output) {
  f.put(output.index());
  if (const auto* u = std::get_if<Update>(&output)) {
    f.put(static_cast<std::uint64_t>(u->kind));
    f.put(u->inst);
    f.put(u->seq);
    f.put(u->prev.has_value() ? *u->prev + 1 : 0);
    f.put(u->epoch);
    put_event(f, u->event);
    f.put(u->level_count);
    for (std::uint8_t i = 0; i < u->level_count; ++i) {
      f.put(static_cast<std::uint64_t>(u->levels[i].key.side));
      f.put(static_cast<std::uint64_t>(u->levels[i].key.price));
      f.put(u->levels[i].total_after);
    }
    for (const auto& [id, order] : u->orders) {
      f.put(id);
      f.put(static_cast<std::uint64_t>(order.side));
      f.put(static_cast<std::uint64_t>(order.price));
      f.put(order.shares);
    }
  } else if (const auto* h = std::get_if<InstrumentHealthChange>(&output)) {
    f.put(h->inst);
    f.put(static_cast<std::uint64_t>(h->state));
  } else if (const auto* fh = std::get_if<FeedHealthChange>(&output)) {
    f.put(static_cast<std::uint64_t>(fh->state));
  } else if (const auto* g = std::get_if<GapDeclared>(&output)) {
    f.put(g->from);
    f.put(g->to);
    f.put(static_cast<std::uint64_t>(g->reason));
  } else if (const auto* a = std::get_if<Alarm>(&output)) {
    f.put(static_cast<std::uint64_t>(a->kind));
    f.put(a->inst);
    f.put(a->seq);
  }
}

}  // namespace

Verdict run_trace(const Trace& trace, std::vector<Output>* outputs) {
  Verdict verdict;
  verdict.output_counts.assign(std::variant_size_v<Output>, 0);

  nasdaq::Handler handler(to_core_config(trace.core));
  fhtest::ConsumerModel consumer(trace.truth.events());
  Fingerprint fingerprint;
  Fingerprint book_fingerprint;  // everything except line-mismatch alarms

  std::vector<Output> out;
  for (const Entry& entry : trace.entries) {
    out.clear();
    switch (entry.kind) {
      case EntryKind::Main:
        handler.on_packet(entry.t, entry.line, entry.bytes, out);
        break;
      case EntryKind::Snapshot:
        handler.on_snapshot_packet(entry.t, entry.bytes, out);
        break;
      case EntryKind::Tick:
        handler.on_tick(entry.t, out);
        break;
    }
    for (const Output& o : out) {
      consumer.consume(o);
      if (outputs != nullptr) outputs->push_back(o);
      put_output(fingerprint, o);
      const auto* alarm = std::get_if<Alarm>(&o);
      if (alarm == nullptr || alarm->kind != AlarmKind::LineMismatch)
        put_output(book_fingerprint, o);
      ++verdict.output_counts[o.index()];
      if (const auto* u = std::get_if<Update>(&o)) {
        verdict.resets += u->kind == UpdateKind::Reset ? 1U : 0U;
      } else if (const auto* g = std::get_if<GapDeclared>(&o)) {
        ++verdict.gap_reasons[static_cast<std::size_t>(g->reason)];
      } else if (const auto* h = std::get_if<FeedHealthChange>(&o)) {
        ++verdict.feed_health[static_cast<std::size_t>(h->state)];
      } else if (const auto* a = std::get_if<Alarm>(&o)) {
        ++verdict.alarms[static_cast<std::size_t>(a->kind)];
      }
    }
  }

  verdict.safety_failures = consumer.failures();
  verdict.updates = consumer.updates_seen();
  verdict.output_digest = fingerprint.value();
  verdict.book_digest = book_fingerprint.value();
  verdict.counters = handler.core().counters();
  verdict.packet_stats = handler.packet_stats();
  verdict.snapshot_stats = handler.snapshot_stats();

  // P4: after the clean tail every instrument must be live and every book right.
  const std::optional<FeedState> final_state = handler.core().export_state();
  if (!final_state) {
    verdict.liveness_failures.push_back(
        "P4: at the end some instrument is not live, or a sequence number is still missing");
  } else {
    verdict.final_checksum = checksum(*final_state);
    const FeedState expected = fhtest::oracle_state(trace.truth.events(), trace.truth.last_seq());
    if (!(*final_state == expected)) {
      verdict.liveness_failures.push_back("P4: the final books differ from the oracle's");
    }
  }
  return verdict;
}

}  // namespace fh::sim
