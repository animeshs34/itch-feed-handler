#ifndef FH_SIM_RECORDED_HPP_
#define FH_SIM_RECORDED_HPP_

#include <array>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <set>
#include <string>

#include "fh/nasdaq/itch_decoder.hpp"
#include "fh/sim/ground_truth.hpp"

namespace fh::sim {

// What was in a recorded file, counted while it was read.
struct Census {
  std::uint64_t records{0};                  // messages read from the file
  std::uint64_t kept{0};                     // messages that went into the ground truth
  std::uint64_t bytes{0};                    // payload bytes of the messages read
  std::uint64_t book_events{0};              // kept messages that change a book
  std::uint64_t unknown_types{0};            // read messages of a type the decoder does not know
  std::array<std::uint64_t, 256> by_type{};  // read messages, by ITCH type letter
  std::map<InstrumentId, std::uint64_t> by_instrument;  // read messages, by stock locate
};

enum class LoadStatus : std::uint8_t {
  Ok,
  Incomplete,     // no end-of-session record: the file may be cut short
  Truncated,      // the stream ended inside a record
  TrailingBytes,  // data after the end-of-session record
  BadMessage,     // a message the decoder refuses; `record` says which
  Empty,          // nothing usable was read
};

struct LoadOptions {
  // Keep only these instruments (by stock locate), plus the market-wide messages (locate 0).
  // Empty means keep everything. The filter starts at the beginning of the file, so every
  // book begins empty; a slice from the middle of the day would not.
  std::set<InstrumentId> only;
  // Stop after keeping this many messages (0 = no limit). A prefix of a valid day is valid.
  std::uint64_t limit{0};
  // Accept a file that has no end-of-session record.
  bool allow_incomplete{false};
};

struct LoadResult {
  LoadStatus status{LoadStatus::Empty};
  GroundTruth truth;
  Census census;
  std::uint64_t record{0};  // 1-based index of the record that stopped the load, if any
  nasdaq::ItchError error{nasdaq::ItchError::None};

  [[nodiscard]] bool ok() const noexcept { return status == LoadStatus::Ok; }
};

[[nodiscard]] std::string to_string(LoadStatus status);

// Reads a BinaryFILE stream of ITCH 5.0 messages into a ground truth.
//
// Each message the decoder accepts becomes a GroundMessage that keeps its exact bytes
// (`raw`), numbered from 1 in the order kept. A book-changing message carries its event;
// every other message is a filler with its type letter and its stock locate. A message the
// decoder refuses stops the load: a day the handler could not read is not a valid
// ground truth.
[[nodiscard]] LoadResult load_recorded(std::istream& in, const LoadOptions& options);

// A fingerprint (FNV-1a, 64 bit) of the messages that were kept: their bytes, in order,
// each preceded by its length. It names the input a result was measured on. It is not
// a hash of the file (the file may be larger than what was kept); record the file's own
// SHA-256 beside it.
[[nodiscard]] std::uint64_t input_digest(const GroundTruth& truth);

}  // namespace fh::sim

#endif  // FH_SIM_RECORDED_HPP_
