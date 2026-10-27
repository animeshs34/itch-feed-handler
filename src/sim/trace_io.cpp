#include "fh/sim/trace_io.hpp"

#include <fstream>
#include <iterator>

namespace fh::sim {
namespace {

class Writer {
 public:
  void u8(std::uint8_t v) { out_.push_back(v); }
  void u16(std::uint16_t v) { put(v, 2); }
  void u32(std::uint32_t v) { put(v, 4); }
  void u64(std::uint64_t v) { put(v, 8); }
  void i64(std::int64_t v) { put(static_cast<std::uint64_t>(v), 8); }
  void bytes(const std::vector<std::uint8_t>& b) { out_.insert(out_.end(), b.begin(), b.end()); }
  std::vector<std::uint8_t> take() { return std::move(out_); }

 private:
  void put(std::uint64_t v, int n) {
    for (int i = 0; i < n; ++i) out_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
  }
  std::vector<std::uint8_t> out_;
};

// Reads fields one by one; once any read fails, every later read fails too and
// `ok()` stays false, so the caller can check once at the end of a group.
class Reader {
 public:
  explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

  std::uint8_t u8() { return static_cast<std::uint8_t>(get(1)); }
  std::uint16_t u16() { return static_cast<std::uint16_t>(get(2)); }
  std::uint32_t u32() { return static_cast<std::uint32_t>(get(4)); }
  std::uint64_t u64() { return get(8); }
  std::int64_t i64() { return static_cast<std::int64_t>(get(8)); }
  std::vector<std::uint8_t> bytes(std::size_t n) {
    if (!ok_ || remaining() < n) {
      ok_ = false;
      return {};
    }
    std::vector<std::uint8_t> out(bytes_.begin() + static_cast<std::ptrdiff_t>(at_),
                                  bytes_.begin() + static_cast<std::ptrdiff_t>(at_ + n));
    at_ += n;
    return out;
  }

  [[nodiscard]] bool ok() const noexcept { return ok_; }
  [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - at_; }
  void fail() noexcept { ok_ = false; }

 private:
  std::uint64_t get(std::size_t n) {
    if (!ok_ || remaining() < n) {
      ok_ = false;
      return 0;
    }
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < n; ++i) v |= static_cast<std::uint64_t>(bytes_[at_ + i]) << (8 * i);
    at_ += n;
    return v;
  }

  std::span<const std::uint8_t> bytes_;
  std::size_t at_{0};
  bool ok_{true};
};

// The smallest a message and an entry can be on the wire, used to refuse counts that
// could not possibly fit in the bytes left.
constexpr std::size_t kMinMessageBytes = 8 + 1 + 1 + 1 + 2;  // version 1; version 2 adds 4
constexpr std::size_t kMinEntryBytes = 8 + 1 + 1 + 4;

}  // namespace

std::vector<std::uint8_t> encode_trace(const Trace& trace) {
  Writer w;
  w.u32(kTraceMagic);
  w.u16(kTraceVersion);
  w.u64(trace.seed);
  w.i64(trace.core.skew_window);
  w.i64(trace.core.silent_window);
  w.u64(trace.core.buffer_capacity);
  w.u64(trace.core.arbiter_window);
  w.u64(trace.core.parked_capacity);

  w.u32(static_cast<std::uint32_t>(trace.truth.messages.size()));
  for (const GroundMessage& m : trace.truth.messages) {
    w.u64(m.seq);
    w.u8(m.event.has_value() ? 1 : 0);
    if (m.event) {
      const BookEvent& e = *m.event;
      w.u8(static_cast<std::uint8_t>(e.kind));
      w.u64(e.seq);
      w.u16(e.inst);
      w.u64(e.order);
      w.u64(e.new_order);
      w.u8(static_cast<std::uint8_t>(e.side));
      w.i64(e.price);
      w.u64(e.shares);
    }
    w.u8(static_cast<std::uint8_t>(m.filler));
    w.u8(static_cast<std::uint8_t>(m.code));
    w.u16(m.locate);
    w.u32(static_cast<std::uint32_t>(m.raw.size()));
    w.bytes(m.raw);
  }

  w.u64(trace.entries.size());
  for (const Entry& e : trace.entries) {
    w.i64(e.t);
    w.u8(static_cast<std::uint8_t>(e.kind));
    w.u8(static_cast<std::uint8_t>(e.line));
    w.u32(static_cast<std::uint32_t>(e.bytes.size()));
    w.bytes(e.bytes);
  }
  return w.take();
}

std::optional<Trace> decode_trace(std::span<const std::uint8_t> bytes) {
  Reader r(bytes);
  if (r.u32() != kTraceMagic) return std::nullopt;
  const std::uint16_t version = r.u16();
  if (version != 1 && version != kTraceVersion) return std::nullopt;

  Trace trace;
  trace.seed = r.u64();
  trace.core.skew_window = r.i64();
  trace.core.silent_window = r.i64();
  trace.core.buffer_capacity = r.u64();
  trace.core.arbiter_window = r.u64();
  trace.core.parked_capacity = r.u64();

  const std::uint32_t message_count = r.u32();
  if (!r.ok() || message_count > r.remaining() / kMinMessageBytes) return std::nullopt;
  trace.truth.messages.reserve(message_count);
  for (std::uint32_t i = 0; i < message_count; ++i) {
    GroundMessage m;
    m.seq = r.u64();
    const std::uint8_t has_event = r.u8();
    if (has_event > 1) return std::nullopt;
    if (has_event == 1) {
      BookEvent e;
      const std::uint8_t kind = r.u8();
      if (kind > static_cast<std::uint8_t>(EventKind::Replace)) return std::nullopt;
      e.kind = static_cast<EventKind>(kind);
      e.seq = r.u64();
      e.inst = r.u16();
      e.order = r.u64();
      e.new_order = r.u64();
      const std::uint8_t side = r.u8();
      if (!is_valid_side(side)) return std::nullopt;
      e.side = static_cast<Side>(side);
      e.price = r.i64();
      e.shares = r.u64();
      m.event = e;
    }
    m.filler = static_cast<char>(r.u8());
    m.code = static_cast<char>(r.u8());
    m.locate = r.u16();
    if (version >= 2) {
      const std::uint32_t raw_length = r.u32();
      m.raw = r.bytes(raw_length);
    }
    if (!r.ok()) return std::nullopt;
    trace.truth.messages.push_back(m);
  }

  const std::uint64_t entry_count = r.u64();
  if (!r.ok() || entry_count > r.remaining() / kMinEntryBytes) return std::nullopt;
  trace.entries.reserve(static_cast<std::size_t>(entry_count));
  for (std::uint64_t i = 0; i < entry_count; ++i) {
    Entry e;
    e.t = r.i64();
    const std::uint8_t kind = r.u8();
    const std::uint8_t line = r.u8();
    if (kind > static_cast<std::uint8_t>(EntryKind::Tick) || line > 1) return std::nullopt;
    e.kind = static_cast<EntryKind>(kind);
    e.line = static_cast<Line>(line);
    const std::uint32_t length = r.u32();
    e.bytes = r.bytes(length);
    if (!r.ok()) return std::nullopt;
    trace.entries.push_back(std::move(e));
  }

  if (!r.ok() || r.remaining() != 0) return std::nullopt;
  return trace;
}

bool write_trace_file(const std::string& path, const Trace& trace) {
  const std::vector<std::uint8_t> bytes = encode_trace(trace);
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(file);
}

std::optional<Trace> read_trace_file(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return std::nullopt;
  const std::vector<char> raw((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
  const std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(raw.data()),
                                            raw.size());
  return decode_trace(bytes);
}

}  // namespace fh::sim
