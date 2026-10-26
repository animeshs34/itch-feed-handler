#include "fh/nasdaq/be_reader.hpp"

#include <cassert>

namespace fh::nasdaq {

std::optional<std::uint64_t> BeReader::take(std::size_t width) noexcept {
  assert(width >= 1 && width <= 8);
  if (remaining() < width) return std::nullopt;
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < width; ++i) {
    value = (value << 8) | static_cast<std::uint64_t>(bytes_[pos_ + i]);
  }
  pos_ += width;
  return value;
}

std::optional<std::uint8_t> BeReader::u8() noexcept {
  const auto v = take(1);
  if (!v) return std::nullopt;
  return static_cast<std::uint8_t>(*v);
}

std::optional<std::uint16_t> BeReader::u16() noexcept {
  const auto v = take(2);
  if (!v) return std::nullopt;
  return static_cast<std::uint16_t>(*v);
}

std::optional<std::uint32_t> BeReader::u32() noexcept {
  const auto v = take(4);
  if (!v) return std::nullopt;
  return static_cast<std::uint32_t>(*v);
}

std::optional<std::uint64_t> BeReader::u64() noexcept { return take(8); }

std::optional<std::span<const std::uint8_t>> BeReader::take_bytes(std::size_t count) noexcept {
  if (remaining() < count) return std::nullopt;
  const auto view = bytes_.subspan(pos_, count);
  pos_ += count;
  return view;
}

bool BeReader::skip(std::size_t count) noexcept {
  if (remaining() < count) return false;
  pos_ += count;
  return true;
}

}  // namespace fh::nasdaq
