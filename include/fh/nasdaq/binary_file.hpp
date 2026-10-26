#ifndef FH_NASDAQ_BINARY_FILE_HPP_
#define FH_NASDAQ_BINARY_FILE_HPP_

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <span>
#include <vector>

#include "fh/nasdaq/itch_encoder.hpp"

namespace fh::nasdaq {

// Nasdaq's BinaryFILE framing, which its historical ITCH 5.0 files use: each record is a
// two-byte big-endian length followed by that many bytes of message, and a record of
// length zero marks the end of the session (Appendix B.6 of the spec).
//
// The reader works on a stream and holds one record at a time, so a file of any size can
// be read. Decompression is not its business: a gzip file is read by piping
// `gunzip -c file.gz` into it.
//
// The framing follows Nasdaq's historical-data guide.
enum class FileRecord : std::uint8_t {
  Message,        // `out` holds one message
  EndOfSession,   // the zero-length terminator
  EndOfFile,      // the stream ended cleanly at a record boundary
  Truncated,      // the stream ended inside a length or inside a message
  TrailingBytes,  // there is more data after the terminator
};

class BinaryFileReader {
 public:
  explicit BinaryFileReader(std::istream& in) : in_(in) {}

  // Reads the next record. For `Message`, `out` is replaced by its bytes; for every other
  // result `out` is left empty.
  [[nodiscard]] FileRecord next(Bytes& out);

  [[nodiscard]] std::uint64_t messages() const noexcept { return messages_; }
  [[nodiscard]] std::uint64_t bytes() const noexcept { return bytes_; }
  // True once the zero-length terminator has been read. A file that reaches EndOfFile
  // without it is incomplete.
  [[nodiscard]] bool saw_end_of_session() const noexcept { return terminated_; }

 private:
  std::istream& in_;
  std::uint64_t messages_{0};
  std::uint64_t bytes_{0};
  bool terminated_{false};
};

class BinaryFileWriter {
 public:
  explicit BinaryFileWriter(std::ostream& out) : out_(out) {}

  // Writes one record. Returns false for an empty message (that would look like the
  // terminator), a message longer than 65,535 bytes, or a failed stream.
  [[nodiscard]] bool write(std::span<const std::uint8_t> message);

  // Writes the zero-length terminator.
  [[nodiscard]] bool write_end_of_session();

 private:
  std::ostream& out_;
};

}  // namespace fh::nasdaq

#endif  // FH_NASDAQ_BINARY_FILE_HPP_
