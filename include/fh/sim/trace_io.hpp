#ifndef FH_SIM_TRACE_IO_HPP_
#define FH_SIM_TRACE_IO_HPP_

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "fh/sim/trace.hpp"

namespace fh::sim {

// The canonical byte form of a trace, so a failing run can be saved, sent to someone
// else, and replayed exactly. Equal traces give equal bytes.
//
//   magic u32 'FHTR' | version u16 (now 2) | seed u64
//   core: skew i64 | silent i64 | buffer u64 | arbiter u64 | parked u64
//   truth: count u32, then per message:
//     seq u64 | has_event u8 | [kind u8 | seq u64 | inst u16 | order u64 | new_order u64 |
//                               side u8 | price i64 | shares u64]  (only if has_event)
//             | filler u8 | code u8 | locate u16
//             | raw_length u32 | raw bytes                       (version 2 only)
//   entries: count u64, then per entry: t i64 | kind u8 | line u8 | length u32 | bytes
//
// All integers are little-endian and fixed width.
//
// Version 1 (Plan 5) had no `raw` field. Version 2 adds it so that a trace made from a
// recorded file carries the file's exact bytes. The reader accepts both; the writer
// always writes version 2.
inline constexpr std::uint32_t kTraceMagic = 0x52544846;  // "FHTR" read as little-endian bytes
inline constexpr std::uint16_t kTraceVersion = 2;

[[nodiscard]] std::vector<std::uint8_t> encode_trace(const Trace& trace);

// Reads a trace back. Returns nothing for anything that is not exactly one well-formed
// trace: wrong magic or version, a value out of range, a count larger than the bytes
// that remain, a truncated field, or bytes left over at the end.
[[nodiscard]] std::optional<Trace> decode_trace(std::span<const std::uint8_t> bytes);

// Whole-file helpers. They return false / nothing on any I/O error.
[[nodiscard]] bool write_trace_file(const std::string& path, const Trace& trace);
[[nodiscard]] std::optional<Trace> read_trace_file(const std::string& path);

}  // namespace fh::sim

#endif  // FH_SIM_TRACE_IO_HPP_
