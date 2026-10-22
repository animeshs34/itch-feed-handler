#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <limits>
#include <vector>

#include "fh/state_codec.hpp"
#include "state_fixtures.hpp"

using Bytes = std::vector<std::uint8_t>;

namespace {

// Appends `x` as `n` little-endian bytes.
void put(Bytes& v, std::uint64_t x, int n) {
  for (int i = 0; i < n; ++i) v.push_back(static_cast<std::uint8_t>(x >> (8 * i)));
}

// The 19-byte header of a valid encoding.
Bytes header(std::uint32_t instrument_count, std::uint64_t seq = 0) {
  Bytes v;
  put(v, 0x4B424846, 4);  // magic "FHBK"
  put(v, 1, 2);           // version
  put(v, 0xFC, 1);        // price_exp = -4
  put(v, seq, 8);
  put(v, instrument_count, 4);
  return v;
}

void put_order(Bytes& v, std::uint64_t id, std::uint8_t side, std::int64_t price,
               std::uint64_t shares) {
  put(v, id, 8);
  put(v, side, 1);
  put(v, static_cast<std::uint64_t>(price), 8);
  put(v, shares, 8);
}

}  // namespace

TEST_CASE("an empty state round trips and is just the header", "[codec]") {
  const Bytes bytes = fh::encode(fh::FeedState{});
  REQUIRE(bytes.size() == 19);
  REQUIRE(fh::decode(bytes).value() == fh::FeedState{});
}

TEST_CASE("an instrument with no orders survives a round trip", "[codec]") {
  fh::FeedState s;
  s.books[5];  // exists, empty: LIVE with nothing resting is a valid state
  s.books[2];
  const auto back = fh::decode(fh::encode(s));
  REQUIRE(back.has_value());
  REQUIRE(*back == s);
  REQUIRE(back->books.size() == 2);
}

TEST_CASE("decode inverts encode including extreme values", "[codec]") {
  fh::FeedState s;
  s.price_exp = -8;
  s.seq = std::numeric_limits<fh::Seq>::max();
  REQUIRE(s.books[65535]
              .apply(fh::make_add(1, 65535, std::numeric_limits<fh::OrderId>::max(), fh::Side::Sell,
                                  std::numeric_limits<fh::Price>::min(),
                                  std::numeric_limits<fh::Shares>::max()))
              .ok());
  REQUIRE(s.books[1].apply(fh::make_add(2, 1, 1, fh::Side::Buy, -5, 3)).ok());
  const auto back = fh::decode(fh::encode(s));
  REQUIRE(back.has_value());
  REQUIRE(*back == s);
}

TEST_CASE("every truncation of a valid encoding is rejected", "[codec][hostile]") {
  const Bytes full = fh::encode(fhtest::one_order_state());
  for (std::size_t len = 0; len < full.size(); ++len) {
    const Bytes cut(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(len));
    INFO("length " << len);
    REQUIRE_FALSE(fh::decode(cut).has_value());
  }
}

TEST_CASE("trailing bytes are rejected", "[codec][hostile]") {
  Bytes bytes = fh::encode(fhtest::one_order_state());
  bytes.push_back(0);
  REQUIRE_FALSE(fh::decode(bytes).has_value());
}

TEST_CASE("wrong magic or version is rejected", "[codec][hostile]") {
  Bytes bad_magic = fh::encode(fh::FeedState{});
  bad_magic[0] ^= 0xFF;
  REQUIRE_FALSE(fh::decode(bad_magic).has_value());

  Bytes bad_version = fh::encode(fh::FeedState{});
  bad_version[4] = 2;
  REQUIRE_FALSE(fh::decode(bad_version).has_value());
}

TEST_CASE("huge counts with almost no data are rejected without large work", "[codec][hostile]") {
  REQUIRE_FALSE(fh::decode(header(0xFFFFFFFF)).has_value());

  Bytes v = header(1);
  put(v, 1, 2);           // instrument id
  put(v, 0xFFFFFFFF, 4);  // claims 4 billion orders
  REQUIRE_FALSE(fh::decode(v).has_value());
}

TEST_CASE("instrument ids must be strictly ascending", "[codec][hostile]") {
  Bytes dup = header(2);
  put(dup, 3, 2);
  put(dup, 0, 4);
  put(dup, 3, 2);
  put(dup, 0, 4);
  REQUIRE_FALSE(fh::decode(dup).has_value());

  Bytes descending = header(2);
  put(descending, 4, 2);
  put(descending, 0, 4);
  put(descending, 3, 2);
  put(descending, 0, 4);
  REQUIRE_FALSE(fh::decode(descending).has_value());

  Bytes ascending = header(2);
  put(ascending, 3, 2);
  put(ascending, 0, 4);
  put(ascending, 4, 2);
  put(ascending, 0, 4);
  REQUIRE(fh::decode(ascending).has_value());
}

TEST_CASE("order ids must be strictly ascending", "[codec][hostile]") {
  Bytes v = header(1);
  put(v, 1, 2);
  put(v, 2, 4);
  put_order(v, 9, 'B', 100, 1);
  put_order(v, 9, 'B', 100, 1);  // same id again
  REQUIRE_FALSE(fh::decode(v).has_value());

  Bytes w = header(1);
  put(w, 1, 2);
  put(w, 2, 4);
  put_order(w, 9, 'B', 100, 1);
  put_order(w, 8, 'B', 100, 1);  // smaller id after a larger one
  REQUIRE_FALSE(fh::decode(w).has_value());
}

TEST_CASE("a bad side byte or zero shares or an overflowing level is rejected",
          "[codec][hostile]") {
  Bytes bad_side = header(1);
  put(bad_side, 1, 2);
  put(bad_side, 1, 4);
  put_order(bad_side, 1, 'X', 100, 1);
  REQUIRE_FALSE(fh::decode(bad_side).has_value());

  Bytes zero_shares = header(1);
  put(zero_shares, 1, 2);
  put(zero_shares, 1, 4);
  put_order(zero_shares, 1, 'B', 100, 0);
  REQUIRE_FALSE(fh::decode(zero_shares).has_value());

  Bytes overflow = header(1);
  put(overflow, 1, 2);
  put(overflow, 2, 4);
  put_order(overflow, 1, 'B', 100, std::numeric_limits<std::uint64_t>::max());
  put_order(overflow, 2, 'B', 100, 1);  // same level: total would wrap
  REQUIRE_FALSE(fh::decode(overflow).has_value());
}
