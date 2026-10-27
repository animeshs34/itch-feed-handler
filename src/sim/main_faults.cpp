#include "fh/sim/main_faults.hpp"

#include <algorithm>
#include <cstring>

#include "fh/nasdaq/moldudp64.hpp"

namespace fh::sim {
namespace {

// The silent window of the core is 6 ms by default; a silence must be clearly longer.
constexpr Nanos kMinSilence = 8 * kMillisecond;
constexpr Nanos kExtraSilence = 12 * kMillisecond;

bool exposed(const Arrival& a, Nanos faulty_until) { return a.entry.t < faulty_until; }

}  // namespace

void apply_silence(std::vector<Arrival>& arrivals, const FaultConfig& faults, Nanos faulty_until,
                   Nanos spacing, fhtest::SplitMix64& rng) {
  for (unsigned w = 0; w < faults.silence_windows; ++w) {
    const Nanos length = kMinSilence + rng.between(0, kExtraSilence);
    if (faulty_until <= length + spacing) continue;
    const Nanos from = spacing + rng.between(0, faulty_until - length - spacing);
    const Nanos to = from + length;
    const std::uint64_t who = rng.below(3);  // 0: line A, 1: line B, 2: both
    arrivals.erase(std::remove_if(arrivals.begin(), arrivals.end(),
                                  [&](const Arrival& a) {
                                    const bool line_hit =
                                        who == 2 || a.entry.line == (who == 0 ? Line::A : Line::B);
                                    return line_hit && a.entry.t >= from && a.entry.t < to;
                                  }),
                   arrivals.end());
  }
}

void apply_malformed(std::vector<Arrival>& arrivals, const FaultConfig& faults, Nanos faulty_until,
                     fhtest::SplitMix64& rng) {
  if (faults.malformed_pct == 0) return;
  for (Arrival& a : arrivals) {
    if (!exposed(a, faulty_until) || !a.genuine) continue;
    if (rng.below(100) >= faults.malformed_pct) continue;
    const auto keep = rng.between(1, static_cast<std::int64_t>(a.entry.bytes.size()) - 1);
    a.entry.bytes.resize(static_cast<std::size_t>(keep));
    a.genuine = false;
  }
}

void add_mismatches(std::vector<Arrival>& arrivals, const FaultConfig& faults, Nanos faulty_until,
                    fhtest::SplitMix64& rng) {
  if (faults.mismatch_pct == 0) return;
  std::vector<Arrival> extra;
  for (const Arrival& a : arrivals) {
    if (!a.genuine || a.dkind != DatagramKind::Messages || !exposed(a, faulty_until)) continue;
    if (rng.below(100) >= faults.mismatch_pct) continue;

    // Walk the blocks to find where the messages are: 20 byte header, then for each
    // message a 2 byte length and the message itself.
    std::vector<std::pair<std::size_t, std::size_t>> messages;  // offset, length
    std::size_t at = nasdaq::kMoldHeaderBytes;
    while (at + 2 <= a.entry.bytes.size()) {
      const std::size_t len =
          (static_cast<std::size_t>(a.entry.bytes[at]) << 8) | a.entry.bytes[at + 1];
      messages.emplace_back(at + 2, len);
      at += 2 + len;
    }
    if (messages.empty()) continue;
    const auto [offset, length] = messages[rng.below(messages.size())];
    if (length < 2) continue;

    Arrival copy = a;
    // Byte 0 is the message type; changing it would just make an unknown type. Any
    // other byte gives a message that either differs or no longer decodes.
    const std::size_t pos =
        offset + static_cast<std::size_t>(rng.between(1, static_cast<std::int64_t>(length) - 1));
    copy.entry.bytes[pos] = static_cast<std::uint8_t>(copy.entry.bytes[pos] ^ (1U << rng.below(8)));
    copy.entry.t += kMicrosecond + rng.between(0, 500 * kMicrosecond);
    copy.genuine = false;
    extra.push_back(std::move(copy));
  }
  arrivals.insert(arrivals.end(), extra.begin(), extra.end());
}

void add_wrong_session(std::vector<Arrival>& arrivals, const FaultConfig& faults,
                       Nanos faulty_until, fhtest::SplitMix64& rng) {
  if (faults.wrong_session_pct == 0) return;
  static constexpr char kOther[] = "WRONGSESS9";
  std::vector<Arrival> extra;
  for (const Arrival& a : arrivals) {
    if (!a.genuine || !exposed(a, faulty_until)) continue;
    if (rng.below(100) >= faults.wrong_session_pct) continue;
    Arrival copy = a;
    std::memcpy(copy.entry.bytes.data(), kOther, 10);
    copy.entry.t += kMicrosecond + rng.between(0, 500 * kMicrosecond);
    copy.genuine = false;
    extra.push_back(std::move(copy));
  }
  arrivals.insert(arrivals.end(), extra.begin(), extra.end());
}

}  // namespace fh::sim
