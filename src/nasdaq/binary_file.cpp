#include "fh/nasdaq/binary_file.hpp"

#include <istream>
#include <limits>
#include <ostream>

namespace fh::nasdaq {

FileRecord BinaryFileReader::next(Bytes& out) {
  out.clear();

  if (terminated_) {
    return in_.peek() == std::istream::traits_type::eof() ? FileRecord::EndOfFile
                                                          : FileRecord::TrailingBytes;
  }

  char header[2];
  in_.read(header, 2);
  const std::streamsize got = in_.gcount();
  if (got == 0) return FileRecord::EndOfFile;
  if (got != 2) return FileRecord::Truncated;

  const std::size_t length = (static_cast<std::size_t>(static_cast<std::uint8_t>(header[0])) << 8) |
                             static_cast<std::uint8_t>(header[1]);
  if (length == 0) {
    terminated_ = true;
    return FileRecord::EndOfSession;
  }

  out.resize(length);
  in_.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(length));
  if (static_cast<std::size_t>(in_.gcount()) != length) {
    out.clear();
    return FileRecord::Truncated;
  }
  ++messages_;
  bytes_ += length;
  return FileRecord::Message;
}

bool BinaryFileWriter::write(std::span<const std::uint8_t> message) {
  if (message.empty() || message.size() > std::numeric_limits<std::uint16_t>::max()) return false;
  const char header[2] = {static_cast<char>(message.size() >> 8),
                          static_cast<char>(message.size() & 0xFF)};
  out_.write(header, 2);
  out_.write(reinterpret_cast<const char*>(message.data()),
             static_cast<std::streamsize>(message.size()));
  return static_cast<bool>(out_);
}

bool BinaryFileWriter::write_end_of_session() {
  const char zero[2] = {0, 0};
  out_.write(zero, 2);
  return static_cast<bool>(out_);
}

}  // namespace fh::nasdaq
