#ifndef FH_BENCH_ENVIRONMENT_HPP_
#define FH_BENCH_ENVIRONMENT_HPP_

#include <string>

#include "fh/time.hpp"

namespace fh::bench {

// What a number was measured on. A latency figure without this is not a result, it is an
// anecdote: the same code on a different machine, kernel, compiler or build type gives
// different numbers, and every report prints this block first.
struct Environment {
  std::string cpu_model;      // from /proc/cpuinfo; "unknown" if it cannot be read
  unsigned logical_cpus{0};   // what the program may see
  std::string cpu_limit;      // the container's CPU limit (cgroup), if there is one
  std::string kernel;         // from uname
  std::string architecture;   // from uname
  std::string compiler;       // the compiler that built the benchmark
  std::string build_type;     // Debug, Release ...
  std::string governor;       // the CPU frequency governor, if the kernel says
  std::string clock_source;   // the kernel's clock source, if it says
  std::string git_hash;       // the commit the benchmark was built from, if known
  int pinned_cpu{-1};         // the CPU the program was pinned to, or -1
  Nanos clock_period{0};      // the clock's tick, as the library states it
  double clock_read_cost{0};  // measured: nanoseconds per reading of the clock
};

// Reads the environment. The clock read cost is measured over a million readings, which
// takes a few milliseconds.
[[nodiscard]] Environment collect_environment();

// Pins the calling thread to one CPU. Returns false if the system refuses (a container may
// not be allowed to, or the CPU may not exist). Call it before collect_environment() so the
// report says so.
[[nodiscard]] bool pin_to_cpu(int cpu);

// A fixed multi-line text block.
[[nodiscard]] std::string render(const Environment& environment);

}  // namespace fh::bench

#endif  // FH_BENCH_ENVIRONMENT_HPP_
