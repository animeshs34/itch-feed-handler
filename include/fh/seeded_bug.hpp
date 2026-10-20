#ifndef FH_SEEDED_BUG_HPP_
#define FH_SEEDED_BUG_HPP_

// Deliberate defects, for checking the checker.
//
// A campaign that reports "no failures" is only worth something if it WOULD have
// reported a failure for a real defect. So the production code carries a few switches,
// off by default, each of which re-creates a classic mistake. Building with
// -DFH_SEEDED_BUG=n (CMake: -DFH_SEEDED_BUG=n) turns bug n on; scripts/check_seeded_bugs.sh
// builds each in turn and requires the campaign to catch it.
//
//   1  a snapshot older than the gap is accepted
//   2  events a snapshot already contains are replayed on top of it
//   3  the last published sequence number is not updated
//   4  a declared gap is reported one number short
//   5  heartbeats are ignored, so lost messages at the end are never noticed
//   6  a fully reduced order stays in the book with zero shares
//   7  a line mismatch is treated as an ordinary duplicate
//
// With the macro at 0 (the default) every FH_BUG(n) is a compile-time false and the
// guarded code is the correct code.
#ifndef FH_SEEDED_BUG
#define FH_SEEDED_BUG 0
#endif

#define FH_BUG(n) (FH_SEEDED_BUG == (n))

#endif  // FH_SEEDED_BUG_HPP_
