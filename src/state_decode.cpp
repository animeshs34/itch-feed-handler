#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <utility>

#include "fh/state_codec.hpp"
#include "state_format.hpp"

namespace fh {

namespace {

// Smallest possible size of one instrument record (id + order_count) and of one
// order record. Used to reject absurd counts before looping over them.
constexpr std::size_t kInstrumentHeaderBytes = 2 + 4;
constexpr std::size_t kOrderBytes = 8 + 1 + 8 + 8;

// Reads little-endian integers from a byte span without ever going past its end.
class Reader {
 public:
  explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

  [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - pos_; }

  template <class U>
  [[nodiscard]] std::optional<U> take() {
    if (remaining() < sizeof(U)) return std::nullopt;
    U value = 0;
    for (std::size_t i = 0; i < sizeof(U); ++i) {
      value = static_cast<U>(value | (static_cast<U>(bytes_[pos_ + i]) << (8 * i)));
    }
    pos_ += sizeof(U);
    return value;
  }

 private:
  std::span<const std::uint8_t> bytes_;
  std::size_t pos_{0};
};

}  // namespace

std::optional<FeedState> decode(std::span<const std::uint8_t> bytes) {
  Reader in(bytes);

  const auto magic = in.take<std::uint32_t>();
  const auto version = in.take<std::uint16_t>();
  const auto price_exp = in.take<std::uint8_t>();
  const auto seq = in.take<std::uint64_t>();
  const auto instrument_count = in.take<std::uint32_t>();
  if (!magic || !version || !price_exp || !seq || !instrument_count) return std::nullopt;
  if (*magic != detail::kStateMagic || *version != detail::kStateVersion) return std::nullopt;
  if (*instrument_count > in.remaining() / kInstrumentHeaderBytes) return std::nullopt;

  FeedState state;
  state.price_exp = static_cast<PriceExp>(*price_exp);
  state.seq = *seq;

  std::optional<InstrumentId> previous_instrument;
  for (std::uint32_t i = 0; i < *instrument_count; ++i) {
    const auto inst = in.take<std::uint16_t>();
    const auto order_count = in.take<std::uint32_t>();
    if (!inst || !order_count) return std::nullopt;
    if (previous_instrument && *inst <= *previous_instrument) return std::nullopt;
    previous_instrument = *inst;
    if (*order_count > in.remaining() / kOrderBytes) return std::nullopt;

    std::map<OrderId, Order> orders;
    std::optional<OrderId> previous_order;
    for (std::uint32_t j = 0; j < *order_count; ++j) {
      const auto id = in.take<std::uint64_t>();
      const auto side = in.take<std::uint8_t>();
      const auto price = in.take<std::uint64_t>();
      const auto shares = in.take<std::uint64_t>();
      if (!id || !side || !price || !shares) return std::nullopt;
      if (previous_order && *id <= *previous_order) return std::nullopt;
      previous_order = *id;
      if (!is_valid_side(*side) || *shares == 0) return std::nullopt;
      orders.emplace_hint(orders.end(), *id,
                          Order{static_cast<Side>(*side), static_cast<Price>(*price), *shares});
    }

    auto book = InstrumentBook::from_orders(std::move(orders));
    if (!book) return std::nullopt;
    state.books.emplace_hint(state.books.end(), *inst, std::move(*book));
  }

  if (in.remaining() != 0) return std::nullopt;  // trailing bytes are not canonical
  return state;
}

}  // namespace fh
