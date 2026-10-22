#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "event_gen.hpp"
#include "feed_state_builder.hpp"
#include "fh/checksum.hpp"
#include "prng.hpp"

// Property P8 from the spec: serialization is the identity and the checksum is
// stable, across many random states. Plus the hostile-input property: if the
// decoder accepts bytes at all, those bytes must be exactly canonical.

TEST_CASE("P8: encode then decode is the identity for 500 random states", "[property]") {
  for (std::uint64_t seed = 1; seed <= 500; ++seed) {
    const auto events = fhtest::generate_valid_events(seed, fhtest::GenConfig{5, 250});
    fh::FeedState state;
    REQUIRE(fhtest::build_state(events, state));

    const std::vector<std::uint8_t> bytes = fh::encode(state);
    const auto back = fh::decode(bytes);
    INFO("seed " << seed);
    REQUIRE(back.has_value());
    REQUIRE(*back == state);
    REQUIRE(fh::encode(*back) == bytes);  // canonical: same bytes again
    REQUIRE(fh::checksum(*back) == fh::checksum(state));
  }
}

TEST_CASE("P8: a state loaded from its orders equals the state built from events", "[property]") {
  for (std::uint64_t seed = 1; seed <= 200; ++seed) {
    fh::FeedState live;
    REQUIRE(
        fhtest::build_state(fhtest::generate_valid_events(seed, fhtest::GenConfig{4, 200}), live));

    fh::FeedState loaded;
    loaded.seq = live.seq;
    for (const auto& [inst, book] : live.books) {
      auto rebuilt = fh::InstrumentBook::from_orders(book.orders());
      REQUIRE(rebuilt.has_value());
      loaded.books.emplace(inst, std::move(*rebuilt));
    }
    INFO("seed " << seed);
    REQUIRE(loaded == live);
    REQUIRE(fh::checksum(loaded) == fh::checksum(live));
  }
}

TEST_CASE("decode never misbehaves on damaged input and accepts only canonical bytes",
          "[property][hostile]") {
  fhtest::SplitMix64 rng(2024);
  std::size_t accepted = 0;
  for (std::uint64_t seed = 1; seed <= 100; ++seed) {
    fh::FeedState state;
    REQUIRE(
        fhtest::build_state(fhtest::generate_valid_events(seed, fhtest::GenConfig{3, 40}), state));
    const std::vector<std::uint8_t> original = fh::encode(state);

    for (int round = 0; round < 200; ++round) {
      std::vector<std::uint8_t> bytes = original;
      switch (rng.below(4)) {
        case 0:  // flip one byte
          bytes[rng.below(bytes.size())] ^= static_cast<std::uint8_t>(1 + rng.below(255));
          break;
        case 1:  // truncate
          bytes.resize(rng.below(bytes.size()));
          break;
        case 2:  // append garbage
          bytes.push_back(static_cast<std::uint8_t>(rng.below(256)));
          break;
        default:  // overwrite a few bytes
          for (int k = 0; k < 3; ++k) {
            bytes[rng.below(bytes.size())] = static_cast<std::uint8_t>(rng.below(256));
          }
          break;
      }
      const auto parsed = fh::decode(bytes);
      if (parsed.has_value()) {
        ++accepted;
        REQUIRE(fh::encode(*parsed) == bytes);  // accepted implies exactly canonical
      }
    }
  }
  // Some mutations (for example changing a price byte) legitimately give another valid state.
  REQUIRE(accepted > 0);
}
