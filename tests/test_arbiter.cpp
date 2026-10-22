#include <catch2/catch_test_macros.hpp>

#include "fh/arbiter.hpp"

using fh::Verdict;

TEST_CASE("the first copy is new and later identical copies are duplicates", "[arbiter]") {
  fh::Arbiter arbiter(100);
  REQUIRE(arbiter.observe(1, 0xAA) == Verdict::New);
  REQUIRE(arbiter.observe(1, 0xAA) == Verdict::Duplicate);
  REQUIRE(arbiter.observe(1, 0xAA) == Verdict::Duplicate);
}

TEST_CASE("a copy with different bytes is a mismatch and the first copy stays", "[arbiter]") {
  fh::Arbiter arbiter(100);
  REQUIRE(arbiter.observe(5, 0xAA) == Verdict::New);
  REQUIRE(arbiter.observe(5, 0xBB) == Verdict::Mismatch);
  REQUIRE(arbiter.observe(5, 0xAA) == Verdict::Duplicate);  // still compares to the first
}

TEST_CASE("different sequence numbers are independent", "[arbiter]") {
  fh::Arbiter arbiter(100);
  REQUIRE(arbiter.observe(1, 1) == Verdict::New);
  REQUIRE(arbiter.observe(2, 1) == Verdict::New);
  REQUIRE(arbiter.observe(3, 1) == Verdict::New);
  REQUIRE(arbiter.remembered() == 3);
}

TEST_CASE("advance forgets old fingerprints and old numbers become unverified", "[arbiter]") {
  fh::Arbiter arbiter(3);  // remember 3 below the expected number
  for (fh::Seq s = 1; s <= 5; ++s) REQUIRE(arbiter.observe(s, s * 10) == Verdict::New);

  arbiter.advance(6);  // keeps 3, 4, 5
  REQUIRE(arbiter.remembered() == 3);
  REQUIRE(arbiter.observe(1, 10) == Verdict::DuplicateUnverified);
  REQUIRE(arbiter.observe(2, 999) == Verdict::DuplicateUnverified);  // cannot even tell it differs
  REQUIRE(arbiter.observe(3, 30) == Verdict::Duplicate);
  REQUIRE(arbiter.observe(4, 999) == Verdict::Mismatch);
}

TEST_CASE("advance does nothing until the expected number passes the window", "[arbiter]") {
  fh::Arbiter arbiter(100);
  REQUIRE(arbiter.observe(1, 1) == Verdict::New);
  arbiter.advance(50);
  REQUIRE(arbiter.remembered() == 1);
  REQUIRE(arbiter.observe(1, 1) == Verdict::Duplicate);
}

TEST_CASE("a number never seen but inside the window is still new", "[arbiter]") {
  fh::Arbiter arbiter(5);
  REQUIRE(arbiter.observe(10, 1) == Verdict::New);
  REQUIRE(arbiter.observe(12, 1) == Verdict::New);
  arbiter.advance(13);                              // keeps 8..12
  REQUIRE(arbiter.observe(11, 1) == Verdict::New);  // the hole at 11 is still fillable
  REQUIRE(arbiter.observe(8, 1) == Verdict::New);   // inside the window
  REQUIRE(arbiter.observe(7, 1) == Verdict::DuplicateUnverified);  // below it
}

TEST_CASE("a window of zero forgets everything below the expected number", "[arbiter]") {
  fh::Arbiter arbiter(0);
  REQUIRE(arbiter.observe(1, 1) == Verdict::New);
  REQUIRE(arbiter.observe(2, 1) == Verdict::New);
  arbiter.advance(3);
  REQUIRE(arbiter.remembered() == 0);
  REQUIRE(arbiter.observe(2, 1) == Verdict::DuplicateUnverified);
}
