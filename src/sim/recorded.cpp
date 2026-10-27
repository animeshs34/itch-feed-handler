#include "fh/sim/recorded.hpp"

#include <istream>

#include "fh/nasdaq/binary_file.hpp"
#include "fh/nasdaq/itch_layout.hpp"

namespace fh::sim {
namespace {

// The stock locate is the two bytes after the type, in every ITCH 5.0 message.
InstrumentId locate_of(const nasdaq::Bytes& message) {
  if (message.size() < 3) return 0;
  return static_cast<InstrumentId>((static_cast<unsigned>(message[1]) << 8) | message[2]);
}

}  // namespace

std::string to_string(LoadStatus status) {
  switch (status) {
    case LoadStatus::Ok:
      return "ok";
    case LoadStatus::Incomplete:
      return "incomplete: no end-of-session record";
    case LoadStatus::Truncated:
      return "truncated: the stream ends inside a record";
    case LoadStatus::TrailingBytes:
      return "trailing bytes after the end-of-session record";
    case LoadStatus::BadMessage:
      return "a message the decoder refuses";
    case LoadStatus::Empty:
      return "empty: no usable message";
  }
  return "?";
}

std::uint64_t input_digest(const GroundTruth& truth) {
  std::uint64_t hash = 14695981039346656037ULL;
  const auto fold = [&hash](std::uint8_t byte) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  };
  for (const GroundMessage& m : truth.messages) {
    const std::size_t n = m.raw.size();
    for (int shift = 0; shift < 32; shift += 8) fold(static_cast<std::uint8_t>(n >> shift));
    for (const std::uint8_t byte : m.raw) fold(byte);
  }
  return hash;
}

LoadResult load_recorded(std::istream& in, const LoadOptions& options) {
  LoadResult result;
  nasdaq::BinaryFileReader reader(in);
  nasdaq::Bytes bytes;

  for (;;) {
    const nasdaq::FileRecord record = reader.next(bytes);
    if (record == nasdaq::FileRecord::EndOfSession) continue;  // the next call says what follows
    if (record == nasdaq::FileRecord::EndOfFile) break;
    if (record == nasdaq::FileRecord::Truncated) {
      result.status = LoadStatus::Truncated;
      result.record = reader.messages() + 1;
      return result;
    }
    if (record == nasdaq::FileRecord::TrailingBytes) {
      result.status = LoadStatus::TrailingBytes;
      return result;
    }

    // A message.
    ++result.census.records;
    result.census.bytes += bytes.size();
    ++result.census.by_type[bytes[0]];
    const nasdaq::ItchResult decoded = nasdaq::decode_itch(bytes);
    if (decoded.kind == nasdaq::ItchKind::Malformed) {
      result.status = LoadStatus::BadMessage;
      result.record = result.census.records;
      result.error = decoded.error;
      return result;
    }
    if (decoded.kind == nasdaq::ItchKind::Unknown) ++result.census.unknown_types;

    const InstrumentId locate =
        decoded.kind == nasdaq::ItchKind::Event ? decoded.event.inst : locate_of(bytes);
    ++result.census.by_instrument[locate];

    const bool wanted = options.only.empty() || locate == 0 || options.only.contains(locate);
    if (!wanted) continue;

    GroundMessage m;
    m.seq = result.truth.messages.size() + 1;
    m.raw = bytes;
    m.locate = locate;
    if (decoded.kind == nasdaq::ItchKind::Event) {
      m.event = decoded.event;
      m.event->seq = m.seq;
      ++result.census.book_events;
    } else {
      m.filler = static_cast<char>(bytes[0]);
      if (bytes[0] == 'S' && bytes.size() > nasdaq::kHeaderBytes) {
        m.code = static_cast<char>(bytes[nasdaq::kHeaderBytes]);
      }
    }
    result.truth.messages.push_back(std::move(m));
    ++result.census.kept;
    if (options.limit != 0 && result.census.kept >= options.limit) {
      result.status = LoadStatus::Ok;  // a prefix is valid whether or not the file was complete
      return result;
    }
  }

  if (result.truth.messages.empty()) {
    result.status = LoadStatus::Empty;
  } else if (!reader.saw_end_of_session() && !options.allow_incomplete) {
    result.status = LoadStatus::Incomplete;
  } else {
    result.status = LoadStatus::Ok;
  }
  return result;
}

}  // namespace fh::sim
