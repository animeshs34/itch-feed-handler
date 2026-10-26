#include "fh/nasdaq/itch_encoder.hpp"

#include <cassert>
#include <cstddef>
#include <string>
#include <utility>

#include "fh/nasdaq/itch_layout.hpp"

namespace fh::nasdaq {

namespace {

// Appends integers in big-endian order, one byte at a time.
class BeWriter {
 public:
  void put(std::uint64_t value, std::size_t width) {
    for (std::size_t i = width; i > 0; --i) {
      bytes_.push_back(static_cast<std::uint8_t>(value >> (8 * (i - 1))));
    }
  }
  void put_char(char c) { bytes_.push_back(static_cast<std::uint8_t>(c)); }
  // Left-justified, padded on the right with spaces, as ITCH alpha fields are.
  void put_alpha(std::string_view text, std::size_t width) {
    for (std::size_t i = 0; i < width; ++i) put_char(i < text.size() ? text[i] : ' ');
  }
  void put_zeros(std::size_t count) { bytes_.insert(bytes_.end(), count, std::uint8_t{0}); }
  Bytes take() { return std::move(bytes_); }

 private:
  Bytes bytes_;
};

// The type byte and the 10 bytes after it that every message shares.
void put_header(BeWriter& w, char type, const ItchHeader& h) {
  w.put_char(type);
  w.put(h.locate, 2);
  w.put(h.tracking, 2);
  w.put(h.timestamp, 6);
}

std::uint32_t narrow32(std::uint64_t value) {
  assert(value <= 0xFFFFFFFFULL);
  return static_cast<std::uint32_t>(value);
}

std::uint32_t price32(fh::Price price) {
  assert(price >= 0 && price <= static_cast<fh::Price>(kMaxPrice4));
  return static_cast<std::uint32_t>(price);
}

}  // namespace

Bytes encode_add(const ItchHeader& h, fh::OrderId order, fh::Side side, fh::Shares shares,
                 std::string_view stock, fh::Price price) {
  BeWriter w;
  put_header(w, 'A', h);
  w.put(order, 8);
  w.put_char(static_cast<char>(side));
  w.put(narrow32(shares), 4);
  w.put_alpha(stock, 8);
  w.put(price32(price), 4);
  return w.take();
}

Bytes encode_add_mpid(const ItchHeader& h, fh::OrderId order, fh::Side side, fh::Shares shares,
                      std::string_view stock, fh::Price price, std::string_view mpid) {
  BeWriter w;
  put_header(w, 'F', h);
  w.put(order, 8);
  w.put_char(static_cast<char>(side));
  w.put(narrow32(shares), 4);
  w.put_alpha(stock, 8);
  w.put(price32(price), 4);
  w.put_alpha(mpid, 4);
  return w.take();
}

Bytes encode_executed(const ItchHeader& h, fh::OrderId order, fh::Shares shares,
                      std::uint64_t match) {
  BeWriter w;
  put_header(w, 'E', h);
  w.put(order, 8);
  w.put(narrow32(shares), 4);
  w.put(match, 8);
  return w.take();
}

Bytes encode_executed_with_price(const ItchHeader& h, fh::OrderId order, fh::Shares shares,
                                 std::uint64_t match, char printable, fh::Price price) {
  BeWriter w;
  put_header(w, 'C', h);
  w.put(order, 8);
  w.put(narrow32(shares), 4);
  w.put(match, 8);
  w.put_char(printable);
  w.put(price32(price), 4);
  return w.take();
}

Bytes encode_cancel(const ItchHeader& h, fh::OrderId order, fh::Shares shares) {
  BeWriter w;
  put_header(w, 'X', h);
  w.put(order, 8);
  w.put(narrow32(shares), 4);
  return w.take();
}

Bytes encode_delete(const ItchHeader& h, fh::OrderId order) {
  BeWriter w;
  put_header(w, 'D', h);
  w.put(order, 8);
  return w.take();
}

Bytes encode_replace(const ItchHeader& h, fh::OrderId old_order, fh::OrderId new_order,
                     fh::Shares shares, fh::Price price) {
  BeWriter w;
  put_header(w, 'U', h);
  w.put(old_order, 8);
  w.put(new_order, 8);
  w.put(narrow32(shares), 4);
  w.put(price32(price), 4);
  return w.take();
}

Bytes encode_system_event(const ItchHeader& h, char event_code) {
  BeWriter w;
  put_header(w, 'S', h);
  w.put_char(event_code);
  return w.take();
}

Bytes encode_stock_directory(const ItchHeader& h, std::string_view stock) {
  BeWriter w;
  put_header(w, 'R', h);
  w.put_alpha(stock, 8);
  w.put_char('Q');      // market category: Nasdaq Global Select
  w.put_char('N');      // financial status: normal
  w.put(100, 4);        // round lot size
  w.put_char('N');      // round lots only
  w.put_char('C');      // issue classification: common stock
  w.put_alpha("Z", 2);  // issue sub-type: not applicable
  w.put_char('P');      // authenticity: live
  w.put_char('N');      // short sale threshold
  w.put_char('N');      // IPO flag
  w.put_char('1');      // LULD reference price tier
  w.put_char('N');      // ETP flag
  w.put(0, 4);          // ETP leverage factor
  w.put_char('N');      // inverse indicator
  return w.take();
}

Bytes encode_trade(const ItchHeader& h, fh::Shares shares, std::string_view stock, fh::Price price,
                   std::uint64_t match) {
  BeWriter w;
  put_header(w, 'P', h);
  w.put(0, 8);      // order reference: always zero in this message
  w.put_char('B');  // buy/sell: always 'B'
  w.put(narrow32(shares), 4);
  w.put_alpha(stock, 8);
  w.put(price32(price), 4);
  w.put(match, 8);
  return w.take();
}

Bytes encode_filler(char type, const ItchHeader& h) {
  const MessageSpec* spec = find_spec(type);
  assert(spec != nullptr && !spec->affects_book);
  BeWriter w;
  put_header(w, type, h);
  w.put_zeros(spec->length - kHeaderBytes);
  return w.take();
}

Bytes encode_end_of_snapshot(fh::Seq resume_seq) {
  const std::string digits = std::to_string(resume_seq);
  assert(digits.size() <= 20);
  BeWriter w;
  w.put_char('G');
  for (std::size_t i = digits.size(); i < 20; ++i) w.put_char(' ');  // right-justified
  for (const char c : digits) w.put_char(c);
  return w.take();
}

std::string synthetic_stock_name(fh::InstrumentId inst) {
  std::string name = "I" + std::to_string(inst);
  name.resize(8, ' ');
  return name;
}

Bytes encode_event(const fh::BookEvent& event, const EventStyle& style) {
  const ItchHeader h{event.inst, style.tracking, style.timestamp};
  switch (event.kind) {
    case fh::EventKind::Add:
      return encode_add(h, event.order, event.side, event.shares, synthetic_stock_name(event.inst),
                        event.price);
    case fh::EventKind::Reduce:
      return style.reduce_as_execute
                 ? encode_executed(h, event.order, event.shares, style.match_number)
                 : encode_cancel(h, event.order, event.shares);
    case fh::EventKind::Delete:
      return encode_delete(h, event.order);
    case fh::EventKind::Replace:
      return encode_replace(h, event.order, event.new_order, event.shares, event.price);
  }
  return {};
}

}  // namespace fh::nasdaq
