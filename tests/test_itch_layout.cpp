#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>

#include "fh/nasdaq/itch_layout.hpp"

using namespace fh::nasdaq;

TEST_CASE("the table has the 23 message types of ITCH 5.0", "[itch_layout]") {
  REQUIRE(kMessageSpecs.size() == 23);
  std::set<char> types;
  for (const MessageSpec& s : kMessageSpecs) types.insert(s.type);
  REQUIRE(types.size() == 23);  // no type appears twice
}

TEST_CASE("every message is at least as long as the common header", "[itch_layout]") {
  for (const MessageSpec& s : kMessageSpecs) {
    INFO("type " << s.type);
    REQUIRE(s.length >= kHeaderBytes);
  }
}

TEST_CASE("exactly seven types change a book", "[itch_layout]") {
  std::string book_types;
  for (const MessageSpec& s : kMessageSpecs) {
    if (s.affects_book) book_types += s.type;
  }
  REQUIRE(book_types == "AFECXDU");
}

TEST_CASE("lengths match the specification tables", "[itch_layout]") {
  REQUIRE(find_spec('A')->length == 36);
  REQUIRE(find_spec('F')->length == 40);
  REQUIRE(find_spec('E')->length == 31);
  REQUIRE(find_spec('C')->length == 36);
  REQUIRE(find_spec('X')->length == 23);
  REQUIRE(find_spec('D')->length == 19);
  REQUIRE(find_spec('U')->length == 35);
  REQUIRE(find_spec('S')->length == 12);
  REQUIRE(find_spec('R')->length == 39);
  REQUIRE(find_spec('P')->length == 44);
  REQUIRE(find_spec('I')->length == 50);
}

TEST_CASE("an unknown type has no spec", "[itch_layout]") {
  REQUIRE(find_spec('Z') == nullptr);
  REQUIRE(find_spec('G') == nullptr);  // the end-of-snapshot message is not an ITCH type
  REQUIRE(find_spec('\0') == nullptr);
}

TEST_CASE("the price limit is 200,000.0000", "[itch_layout]") {
  REQUIRE(kMaxPrice4 == 0x77359400u);
}

TEST_CASE("the table can be searched at compile time", "[itch_layout]") {
  static_assert(find_spec('A')->length == 36);
  static_assert(find_spec('A')->affects_book);
  static_assert(find_spec('!') == nullptr);
  REQUIRE(true);
}
