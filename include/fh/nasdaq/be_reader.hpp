#ifndef FH_NASDAQ_BE_READER_HPP_
#define FH_NASDAQ_BE_READER_HPP_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace fh::nasdaq {

// Reads big-endian (network order) integers and byte runs from a span, one
// after another, without ever going past its end. Every read says whether it
// succeeded, so a short or damaged message can never be read out of bounds.
//
// Big-endian is what Nasdaq's MoldUDP64 and ITCH use; the project's own state
// format (Plan 1) is little-endian, so the two readers are deliberately
// separate.
class BeReader {
 public:
  explicit BeReader(std::span<const std::uint8_t> bytes) noexcept : bytes_(bytes) {}

  [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - pos_; }
  [[nodiscard]] std::size_t position() const noexcept { return pos_; }

  // An unsigned integer of `width` bytes, 1 to 8 (ITCH timestamps are 6).
  [[nodiscard]] std::optional<std::uint64_t> take(std::size_t width) noexcept;

  [[nodiscard]] std::optional<std::uint8_t> u8() noexcept;
  [[nodiscard]] std::optional<std::uint16_t> u16() noexcept;
  [[nodiscard]] std::optional<std::uint32_t> u32() noexcept;
  [[nodiscard]] std::optional<std::uint64_t> u64() noexcept;

  // The next `count` bytes, as a view into the original span.
  [[nodiscard]] std::optional<std::span<const std::uint8_t>> take_bytes(std::size_t count) noexcept;

  // Moves past `count` bytes. Returns false (and moves nothing) if too few remain.
  [[nodiscard]] bool skip(std::size_t count) noexcept;

 private:
  std::span<const std::uint8_t> bytes_;
  std::size_t pos_{0};
};

}  // namespace fh::nasdaq

#endif  // FH_NASDAQ_BE_READER_HPP_
