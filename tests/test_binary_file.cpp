#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "fh/nasdaq/binary_file.hpp"
#include "hex.hpp"

using fh::nasdaq::Bytes;
using fh::nasdaq::FileRecord;

namespace {

std::string as_text(const Bytes& b) { return std::string(b.begin(), b.end()); }

}  // namespace

TEST_CASE("a record is a two-byte big-endian length then the message", "[binary-file]") {
  std::istringstream in(
      std::string("\x00\x03"
                  "abc"
                  "\x00\x01"
                  "z",
                  8));
  fh::nasdaq::BinaryFileReader reader(in);
  Bytes out;
  REQUIRE(reader.next(out) == FileRecord::Message);
  REQUIRE(as_text(out) == "abc");
  REQUIRE(reader.next(out) == FileRecord::Message);
  REQUIRE(as_text(out) == "z");
  REQUIRE(reader.next(out) == FileRecord::EndOfFile);
  REQUIRE(reader.messages() == 2);
  REQUIRE(reader.bytes() == 4);
  REQUIRE_FALSE(reader.saw_end_of_session());
}

TEST_CASE("a length above 255 uses both length bytes", "[binary-file]") {
  std::string file = std::string("\x01\x2C", 2) + std::string(300, 'q');
  std::istringstream in(file);
  fh::nasdaq::BinaryFileReader reader(in);
  Bytes out;
  REQUIRE(reader.next(out) == FileRecord::Message);
  REQUIRE(out.size() == 300);
}

TEST_CASE("a zero-length record is the end of the session", "[binary-file]") {
  std::istringstream in(
      std::string("\x00\x01"
                  "a"
                  "\x00\x00",
                  5));
  fh::nasdaq::BinaryFileReader reader(in);
  Bytes out;
  REQUIRE(reader.next(out) == FileRecord::Message);
  REQUIRE(reader.next(out) == FileRecord::EndOfSession);
  REQUIRE(reader.saw_end_of_session());
  REQUIRE(reader.next(out) == FileRecord::EndOfFile);
}

TEST_CASE("bytes after the terminator are reported", "[binary-file]") {
  std::istringstream in(std::string("\x00\x00\x00\x01x", 5));
  fh::nasdaq::BinaryFileReader reader(in);
  Bytes out;
  REQUIRE(reader.next(out) == FileRecord::EndOfSession);
  REQUIRE(reader.next(out) == FileRecord::TrailingBytes);
}

TEST_CASE("a stream that ends inside a record is truncated", "[binary-file]") {
  Bytes out;
  {
    std::istringstream in(std::string("\x00", 1));  // half a length
    fh::nasdaq::BinaryFileReader reader(in);
    REQUIRE(reader.next(out) == FileRecord::Truncated);
  }
  {
    std::istringstream in(
        std::string("\x00\x05"
                    "ab",
                    4));  // 2 of 5 payload bytes
    fh::nasdaq::BinaryFileReader reader(in);
    REQUIRE(reader.next(out) == FileRecord::Truncated);
    REQUIRE(out.empty());
  }
}

TEST_CASE("an empty stream is a clean end of file with no terminator", "[binary-file]") {
  std::istringstream in("");
  fh::nasdaq::BinaryFileReader reader(in);
  Bytes out;
  REQUIRE(reader.next(out) == FileRecord::EndOfFile);
  REQUIRE_FALSE(reader.saw_end_of_session());
}

TEST_CASE("what the writer writes the reader reads", "[binary-file]") {
  std::stringstream stream;
  fh::nasdaq::BinaryFileWriter writer(stream);
  const Bytes a = fhtest::from_hex("41 00 01");
  const Bytes b(1000, 0x7E);
  REQUIRE(writer.write(a));
  REQUIRE(writer.write(b));
  REQUIRE(writer.write_end_of_session());

  fh::nasdaq::BinaryFileReader reader(stream);
  Bytes out;
  REQUIRE(reader.next(out) == FileRecord::Message);
  REQUIRE(out == a);
  REQUIRE(reader.next(out) == FileRecord::Message);
  REQUIRE(out == b);
  REQUIRE(reader.next(out) == FileRecord::EndOfSession);
  REQUIRE(reader.next(out) == FileRecord::EndOfFile);
}

TEST_CASE("the writer refuses what the framing cannot hold", "[binary-file]") {
  std::stringstream stream;
  fh::nasdaq::BinaryFileWriter writer(stream);
  REQUIRE_FALSE(writer.write(Bytes{}));          // would look like the terminator
  REQUIRE_FALSE(writer.write(Bytes(65536, 1)));  // length does not fit in two bytes
  REQUIRE(writer.write(Bytes(65535, 1)));        // the largest that does
}

TEST_CASE("the exact bytes of a small file", "[binary-file]") {
  std::stringstream stream;
  fh::nasdaq::BinaryFileWriter writer(stream);
  REQUIRE(writer.write(fhtest::from_hex("53 00 00")));
  REQUIRE(writer.write_end_of_session());
  const std::string bytes = stream.str();
  REQUIRE(bytes == std::string("\x00\x03\x53\x00\x00\x00\x00", 7));
}
