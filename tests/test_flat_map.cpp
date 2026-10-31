#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <map>

#include "fh/flat_map.hpp"
#include "prng.hpp"

namespace {

// A deliberately poor hash: only 8 distinct home slots, so every probe chain is long and
// erase has to shift entries back across wrap-around.
struct CollidingHash {
  std::uint64_t operator()(std::uint64_t key) const noexcept { return key % 8; }
};
struct GoodHash {
  std::uint64_t operator()(std::uint64_t key) const noexcept { return fh::mix64(key); }
};

template <typename Hash>
void compare_with_std_map(std::uint64_t seed, std::uint64_t key_space, int steps) {
  fh::FlatMap<std::uint64_t, std::uint64_t, Hash> flat;
  std::map<std::uint64_t, std::uint64_t> model;
  fhtest::SplitMix64 rng(seed);
  for (int i = 0; i < steps; ++i) {
    const std::uint64_t key = rng.below(key_space);
    switch (rng.below(3)) {
      case 0: {
        const std::uint64_t value = rng.next();
        REQUIRE(flat.insert(key, value) == model.emplace(key, value).second);
        break;
      }
      case 1:
        REQUIRE(flat.erase(key) == (model.erase(key) == 1));
        break;
      default: {
        const auto* found = flat.find(key);
        const auto it = model.find(key);
        REQUIRE((found != nullptr) == (it != model.end()));
        if (found) REQUIRE(*found == it->second);
      }
    }
    REQUIRE(flat.size() == model.size());
  }
  // Every entry the model has is in the table and nothing else is.
  std::size_t seen = 0;
  flat.for_each([&](std::uint64_t key, std::uint64_t value) {
    ++seen;
    REQUIRE(model.at(key) == value);
  });
  REQUIRE(seen == model.size());
}

}  // namespace

TEST_CASE("FlatMap agrees with std::map under a random mix of operations", "[flat_map]") {
  for (std::uint64_t seed = 1; seed <= 20; ++seed) {
    compare_with_std_map<GoodHash>(seed, 500, 4000);
  }
}

TEST_CASE("FlatMap erase is correct when every key collides", "[flat_map]") {
  for (std::uint64_t seed = 1; seed <= 20; ++seed) {
    compare_with_std_map<CollidingHash>(seed, 60, 3000);
  }
}

TEST_CASE("FlatMap insert keeps the old value and get_or_insert adds in place", "[flat_map]") {
  fh::FlatMap<std::uint64_t, std::uint64_t, GoodHash> flat;
  REQUIRE(flat.insert(1, 10));
  REQUIRE_FALSE(flat.insert(1, 99));
  REQUIRE(*flat.find(1) == 10);
  flat.get_or_insert(2, 0) += 5;
  flat.get_or_insert(2, 0) += 5;
  REQUIRE(*flat.find(2) == 10);
  REQUIRE_FALSE(flat.contains(3));
  REQUIRE_FALSE(flat.erase(3));
}
