#include <cstddef>
#include <cstdint>
#include <vector>

#include "fh/state_codec.hpp"
#include "state_format.hpp"

namespace fh {

namespace {

// Appends `value` as sizeof(U) little-endian bytes, whatever the host's byte order.
template <class U>
void put(std::vector<std::uint8_t>& out, U value) {
  for (std::size_t i = 0; i < sizeof(U); ++i) {
    out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
  }
}

}  // namespace

std::vector<std::uint8_t> encode(const FeedState& state) {
  std::vector<std::uint8_t> out;
  put<std::uint32_t>(out, detail::kStateMagic);
  put<std::uint16_t>(out, detail::kStateVersion);
  put<std::uint8_t>(out, static_cast<std::uint8_t>(state.price_exp));
  put<std::uint64_t>(out, state.seq);
  put<std::uint32_t>(out, static_cast<std::uint32_t>(state.books.size()));
  for (const auto& [inst, book] : state.books) {
    put<std::uint16_t>(out, inst);
    put<std::uint32_t>(out, static_cast<std::uint32_t>(book.orders().size()));
    for (const auto& [id, order] : book.orders()) {
      put<std::uint64_t>(out, id);
      put<std::uint8_t>(out, static_cast<std::uint8_t>(order.side));
      put<std::uint64_t>(out, static_cast<std::uint64_t>(order.price));
      put<std::uint64_t>(out, order.shares);
    }
  }
  return out;
}

}  // namespace fh
