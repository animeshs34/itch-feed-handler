#include "fh/nasdaq/itch_decoder.hpp"

#include <optional>

#include "fh/nasdaq/be_reader.hpp"
#include "fh/nasdaq/itch_layout.hpp"

namespace fh::nasdaq {

namespace {

// Reads a side byte; returns nullopt unless it is 'B' or 'S'.
std::optional<fh::Side> read_side(BeReader& in) {
  const auto raw = in.u8();
  if (!raw || !fh::is_valid_side(*raw)) return std::nullopt;
  return static_cast<fh::Side>(*raw);
}

// Reads a Price(4) and checks it against the specification's limit.
std::optional<fh::Price> read_price(BeReader& in) {
  const auto raw = in.u32();
  if (!raw || *raw > kMaxPrice4) return std::nullopt;
  return static_cast<fh::Price>(*raw);
}

ItchResult fail(ItchResult result, ItchError error) {
  result.kind = ItchKind::Malformed;
  result.error = error;
  return result;
}

}  // namespace

ItchResult decode_itch(std::span<const std::uint8_t> message) {
  ItchResult result;
  if (message.empty()) return fail(result, ItchError::Empty);

  result.type = static_cast<char>(message[0]);
  const MessageSpec* spec = find_spec(result.type);
  if (spec == nullptr) {
    result.kind = ItchKind::Unknown;
    return result;
  }
  if (message.size() != spec->length) return fail(result, ItchError::WrongLength);
  if (!spec->affects_book) {
    result.kind = ItchKind::NoEffect;
    return result;
  }

  // Every book-changing message starts with the same header.
  BeReader in(message);
  (void)in.skip(1);  // the type byte, already read
  const auto locate = in.u16();
  (void)in.skip(2);  // tracking number: Nasdaq-internal, ignored
  (void)in.skip(6);  // timestamp: carried by the exchange, not used by the core
  // The length was checked above, so the header reads cannot run out of bytes.

  fh::BookEvent event;
  event.inst = *locate;

  switch (result.type) {
    case 'A':
    case 'F': {
      const auto order = in.u64();
      const auto side = read_side(in);
      const auto shares = in.u32();
      (void)in.skip(8);  // stock symbol: the locate already identifies the instrument
      const auto price = read_price(in);
      // 'F' continues with a 4 byte market participant id, which is ignored.
      if (!side) return fail(result, ItchError::BadSide);
      if (!price) return fail(result, ItchError::BadPrice);
      if (*shares == 0) return fail(result, ItchError::ZeroShares);
      event = fh::make_add(0, event.inst, *order, *side, *price, *shares);
      break;
    }
    case 'E':
    case 'C':
    case 'X': {
      const auto order = in.u64();
      const auto shares = in.u32();
      if (*shares == 0) return fail(result, ItchError::ZeroShares);
      if (result.type == 'C') {
        (void)in.skip(8);  // match number
        const auto printable = in.u8();
        const auto price = read_price(in);
        if (*printable != 'Y' && *printable != 'N') return fail(result, ItchError::BadFlag);
        if (!price) return fail(result, ItchError::BadPrice);
      }
      event = fh::make_reduce(0, event.inst, *order, *shares);
      break;
    }
    case 'D': {
      const auto order = in.u64();
      event = fh::make_delete(0, event.inst, *order);
      break;
    }
    case 'U': {
      const auto old_order = in.u64();
      const auto new_order = in.u64();
      const auto shares = in.u32();
      const auto price = read_price(in);
      if (!price) return fail(result, ItchError::BadPrice);
      if (*shares == 0) return fail(result, ItchError::ZeroShares);
      event = fh::make_replace(0, event.inst, *old_order, *new_order, *price, *shares);
      break;
    }
    default:
      // Unreachable: the spec table marks exactly the types above as book-changing.
      return fail(result, ItchError::WrongLength);
  }

  result.kind = ItchKind::Event;
  result.event = event;
  return result;
}

}  // namespace fh::nasdaq
