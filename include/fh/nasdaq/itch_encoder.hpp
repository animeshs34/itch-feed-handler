#ifndef FH_NASDAQ_ITCH_ENCODER_HPP_
#define FH_NASDAQ_ITCH_ENCODER_HPP_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "fh/events.hpp"
#include "fh/types.hpp"

namespace fh::nasdaq {

using Bytes = std::vector<std::uint8_t>;

// The three fields every ITCH message starts with after its type byte.
struct ItchHeader {
  fh::InstrumentId locate{0};
  std::uint16_t tracking{0};
  std::uint64_t timestamp{0};  // nanoseconds since midnight; only the low 48 bits are written
};

// Builds ITCH 5.0 messages as bytes. The decoder's mirror image, used by tests
// and by the simulator to make data in exactly the real format.
//
// Share counts and prices are 32-bit on the wire; callers must keep to that
// range (prices at most 2,000,000,000).
[[nodiscard]] Bytes encode_add(const ItchHeader& h, fh::OrderId order, fh::Side side,
                               fh::Shares shares, std::string_view stock, fh::Price price);
[[nodiscard]] Bytes encode_add_mpid(const ItchHeader& h, fh::OrderId order, fh::Side side,
                                    fh::Shares shares, std::string_view stock, fh::Price price,
                                    std::string_view mpid);
[[nodiscard]] Bytes encode_executed(const ItchHeader& h, fh::OrderId order, fh::Shares shares,
                                    std::uint64_t match);
[[nodiscard]] Bytes encode_executed_with_price(const ItchHeader& h, fh::OrderId order,
                                               fh::Shares shares, std::uint64_t match,
                                               char printable, fh::Price price);
[[nodiscard]] Bytes encode_cancel(const ItchHeader& h, fh::OrderId order, fh::Shares shares);
[[nodiscard]] Bytes encode_delete(const ItchHeader& h, fh::OrderId order);
[[nodiscard]] Bytes encode_replace(const ItchHeader& h, fh::OrderId old_order,
                                   fh::OrderId new_order, fh::Shares shares, fh::Price price);

// Messages that change no book.
[[nodiscard]] Bytes encode_system_event(const ItchHeader& h, char event_code);
[[nodiscard]] Bytes encode_stock_directory(const ItchHeader& h, std::string_view stock);
[[nodiscard]] Bytes encode_trade(const ItchHeader& h, fh::Shares shares, std::string_view stock,
                                 fh::Price price, std::uint64_t match);
// Any known no-effect type, with the right length and a valid header; the body is zeros.
[[nodiscard]] Bytes encode_filler(char type, const ItchHeader& h);

// The end-of-snapshot message of the snapshot stream: type 'G' and a 20 character
// ASCII number, right-justified with spaces (Appendix B.5). `resume_seq` is the
// sequence number the live feed should be applied from, which is the snapshot's
// as-of plus one.
[[nodiscard]] Bytes encode_end_of_snapshot(fh::Seq resume_seq);

// Style choices when turning a BookEvent into a message: a Reduce can be written
// as an execution ('E') or a cancel ('X').
struct EventStyle {
  bool reduce_as_execute{false};
  std::uint64_t match_number{0};  // used for executions
  std::uint64_t timestamp{0};
  std::uint16_t tracking{0};
};

// The ITCH message for a book event. The stock name is made from the instrument
// id ("I" followed by the id in decimal, padded), so the same instrument always
// has the same name.
[[nodiscard]] Bytes encode_event(const fh::BookEvent& event, const EventStyle& style);

// The 8 character, space-padded stock name used for an instrument id.
[[nodiscard]] std::string synthetic_stock_name(fh::InstrumentId inst);

}  // namespace fh::nasdaq

#endif  // FH_NASDAQ_ITCH_ENCODER_HPP_
