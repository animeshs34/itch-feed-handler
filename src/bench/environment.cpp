#include "fh/bench/environment.hpp"

#include <sched.h>
#include <sys/utsname.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <thread>

#ifndef FH_BUILD_TYPE
#define FH_BUILD_TYPE "unknown"
#endif
#ifndef FH_GIT_HASH
#define FH_GIT_HASH "unknown"
#endif

namespace fh::bench {
namespace {

std::string cpu_model_from_proc() {
  std::ifstream in("/proc/cpuinfo");
  std::string line;
  std::string implementer, part;
  while (std::getline(in, line)) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) continue;
    const std::string key = line.substr(0, line.find_last_not_of(" \t", colon - 1) + 1);
    const std::string value = colon + 2 <= line.size() ? line.substr(colon + 2) : "";
    if (key == "model name") return value;
    if (key == "CPU implementer" && implementer.empty()) implementer = value;
    if (key == "CPU part" && part.empty()) part = value;
  }
  // Arm kernels do not give a model name; the implementer and part numbers identify the core.
  if (!implementer.empty()) return "arm implementer " + implementer + " part " + part;
  return "unknown";
}

std::string cpu_limit_from_cgroup() {
  std::ifstream in("/sys/fs/cgroup/cpu.max");  // cgroup v2: "max 100000" or "200000 100000"
  std::string quota, period;
  if (!(in >> quota >> period)) return "none found";
  if (quota == "max") return "none (unlimited)";
  return quota + " us per " + period + " us";
}

// The first line of a small text file, or `fallback` if it cannot be read.
std::string first_line(const char* path, const char* fallback) {
  std::ifstream in(path);
  std::string line;
  if (in && std::getline(in, line) && !line.empty()) return line;
  return fallback;
}

}  // namespace

bool pin_to_cpu(int cpu) {
  if (cpu < 0 || cpu >= CPU_SETSIZE) return false;
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(static_cast<std::size_t>(cpu), &set);
  return ::sched_setaffinity(0, sizeof set, &set) == 0;
}

Environment collect_environment() {
  Environment e;
  e.cpu_model = cpu_model_from_proc();
  e.logical_cpus = std::thread::hardware_concurrency();
  e.cpu_limit = cpu_limit_from_cgroup();
  e.governor = first_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor",
                          "not reported (virtualised or no cpufreq)");
  e.clock_source = first_line("/sys/devices/system/clocksource/clocksource0/current_clocksource",
                              "not reported");
  e.git_hash = FH_GIT_HASH;
  // Which CPU, if the affinity mask is a single one.
  cpu_set_t set;
  CPU_ZERO(&set);
  if (::sched_getaffinity(0, sizeof set, &set) == 0 && CPU_COUNT(&set) == 1) {
    for (std::size_t c = 0; c < CPU_SETSIZE; ++c) {
      if (CPU_ISSET(c, &set)) e.pinned_cpu = static_cast<int>(c);
    }
  }

  utsname name{};
  if (::uname(&name) == 0) {
    e.kernel = std::string(name.sysname) + " " + name.release;
    e.architecture = name.machine;
  } else {
    e.kernel = "unknown";
    e.architecture = "unknown";
  }

#if defined(__clang__)
  e.compiler = std::string("clang ") + __clang_version__;
#elif defined(__GNUC__)
  e.compiler = std::string("gcc ") + __VERSION__;
#else
  e.compiler = "unknown";
#endif
  e.build_type = FH_BUILD_TYPE;

  using Clock = std::chrono::steady_clock;
  e.clock_period =
      static_cast<Nanos>(Clock::period::num) * kSecond / static_cast<Nanos>(Clock::period::den);

  constexpr int kReads = 1'000'000;
  Clock::time_point sink{};
  const auto begin = Clock::now();
  for (int i = 0; i < kReads; ++i) sink = Clock::now();
  const auto end = Clock::now();
  (void)sink;
  e.clock_read_cost =
      static_cast<double>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()) /
      kReads;
  return e;
}

std::string render(const Environment& e) {
  char line[200];
  std::string out;
  const auto put = [&](const char* label, const std::string& value) {
    std::snprintf(line, sizeof line, "  %-18s %s\n", label, value.c_str());
    out += line;
  };
  out += "environment:\n";
  put("cpu", e.cpu_model);
  put("logical cpus", std::to_string(e.logical_cpus));
  put("cpu limit", e.cpu_limit);
  put("kernel", e.kernel + " (" + e.architecture + ")");
  put("compiler", e.compiler);
  put("build type", e.build_type);
  put("git commit", e.git_hash);
  put("cpu governor", e.governor);
  put("clock source", e.clock_source);
  put("pinned", e.pinned_cpu >= 0 ? "to cpu " + std::to_string(e.pinned_cpu) : std::string("no"));
  std::snprintf(line, sizeof line, "%lld ns tick, %.1f ns per reading",
                static_cast<long long>(e.clock_period), e.clock_read_cost);
  put("clock", line);
  return out;
}

}  // namespace fh::bench
