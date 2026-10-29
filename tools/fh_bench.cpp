// fh_bench: measures the handler. It prints the environment first, then the numbers.
//
//   fh_bench env
//   fh_bench closed   [--events N] [--repeats R] [--pack K] [common options]
//   fh_bench sweep    [--stage handler|core] [--rates r1,r2,...] [--count C] [--runs R] [common
//   options] fh_bench e2e      [--rates r1,r2,...] [--count C] [--runs R] [common options] fh_bench
//   recovery [--tier clean|mild|moderate|heavy|brutal] [--scenarios N] fh_bench all      (closed,
//   sweep, e2e and recovery with the default settings)
//
// Common options: --events N (size of the synthetic day), --file FILE|- [--instruments 1,2,3]
// [--limit N] (use a recorded BinaryFILE day instead), --pin CPU.
//
// Rates are datagrams (one message each) per second. Build in Release mode for numbers that
// mean anything; the report says what it was built as.
//
// Exit status: 0 done, 2 bad arguments or a benchmark that could not run.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "fh/bench/closed_loop.hpp"
#include "fh/bench/end_to_end.hpp"
#include "fh/bench/environment.hpp"
#include "fh/bench/rate_sweep.hpp"
#include "fh/bench/recovery.hpp"
#include "fh/bench/report.hpp"
#include "fh/sim/consistency.hpp"
#include "fh/sim/recorded.hpp"

namespace {

using namespace fh::bench;

int usage() {
  std::cerr
      << "usage: fh_bench env | closed | sweep | e2e | recovery | all\n"
         "       [--events N] [--repeats R] [--pack K] [--stage handler|core] [--rates r1,r2,...]\n"
         "       [--count C] [--runs R] [--tier T] [--scenarios N] [--pin CPU]\n"
         "       [--file FILE|- [--instruments 1,2,3] [--limit N]]\n";
  return 2;
}

struct Options {
  std::size_t events{200000};
  unsigned repeats{3};
  unsigned pack{1};
  Stage stage{Stage::Handler};
  std::vector<double> rates{50000, 100000, 200000, 400000, 800000, 1600000};
  std::uint64_t count{200000};
  unsigned runs{1};
  fh::sim::Tier tier{fh::sim::Tier::Moderate};
  std::uint64_t scenarios{200};
  int pin{-1};
  std::string file;
  fh::sim::LoadOptions load;
};

bool parse_list(const std::string& text, std::vector<double>& out) {
  std::vector<double> values;
  std::stringstream in(text);
  std::string item;
  while (std::getline(in, item, ',')) {
    char* end = nullptr;
    const double v = std::strtod(item.c_str(), &end);
    if (item.empty() || *end != '\0' || v <= 0) return false;
    values.push_back(v);
  }
  if (values.empty()) return false;
  out = values;
  return true;
}

// The synthetic day, or the recorded one if --file was given.
bool make_workload_for(const Options& o, unsigned pack, Workload& out, std::string& what) {
  if (o.file.empty()) {
    out = make_workload(o.events, 50, 1, pack);
    what = "synthetic day";
    return true;
  }
  std::ifstream file;
  std::istream* in = &std::cin;
  if (o.file != "-") {
    file.open(o.file, std::ios::binary);
    if (!file) {
      std::cerr << "fh_bench: cannot open " << o.file << "\n";
      return false;
    }
    in = &file;
  }
  fh::sim::LoadResult loaded = fh::sim::load_recorded(*in, o.load);
  if (!loaded.ok() || !fh::sim::check_consistency(loaded.truth).ok()) {
    std::cerr << "fh_bench: the recorded file cannot be used (" << fh::sim::to_string(loaded.status)
              << ")\n";
    return false;
  }
  out = make_workload(loaded.truth, 1, pack);
  what = "recorded day, input digest " + std::to_string(fh::sim::input_digest(loaded.truth));
  return true;
}

void run_closed(const Options& o) {
  Workload w;
  std::string what;
  if (!make_workload_for(o, o.pack, w, what)) std::exit(2);
  std::cout << "closed loop on a " << what << ": " << w.messages << " messages in "
            << w.datagrams.size() << " datagrams (" << w.bytes / w.datagrams.size()
            << " bytes each on average), " << o.repeats
            << " timed passes\n  one timed call per datagram; each includes two clock readings\n\n";
  const ClosedLoopResult handler = bench_handler_closed_loop(w, o.repeats);
  const ClosedLoopResult core = bench_core_closed_loop(w, o.repeats);
  std::cout << percentile_header();
  std::cout << percentile_row("handler (bytes -> out)", summarize(handler.service));
  std::cout << percentile_row("core only (decoded)", summarize(core.service));
  std::cout << "\nthroughput (an untimed pass):\n";
  std::printf("  handler  %s messages, %s datagrams, %llu outputs\n",
              format_rate(handler.messages_per_second).c_str(),
              format_rate(handler.datagrams_per_second).c_str(),
              static_cast<unsigned long long>(handler.outputs));
  std::printf("  core     %s messages, %s datagrams\n\n",
              format_rate(core.messages_per_second).c_str(),
              format_rate(core.datagrams_per_second).c_str());
}

void print_sweep_header() {
  std::printf("%-10s %-10s %7s %-26s %-26s %-26s\n", "offered", "achieved", "late %",
              "p50 (min / median / max)", "p99 (min / median / max)", "max (min / median / max)");
}

void print_spread_row(double offered, double achieved, double late_pct,
                      const std::vector<double>& p50, const std::vector<double>& p99,
                      const std::vector<double>& max) {
  const Spread a = spread_of(p50);
  const Spread b = spread_of(p99);
  const Spread c = spread_of(max);
  const auto cell = [](const Spread& s) {
    return format_ns(s.min) + " / " + format_ns(s.median) + " / " + format_ns(s.max);
  };
  std::printf("%-10s %-10s %6.1f%% %-26s %-26s %-26s\n", format_rate(offered).c_str(),
              format_rate(achieved).c_str(), late_pct, cell(a).c_str(), cell(b).c_str(),
              cell(c).c_str());
}

void run_sweep(const Options& o) {
  Workload w;
  std::string what;
  if (!make_workload_for(o, 1, w, what)) std::exit(2);
  const std::uint64_t warmup = 5000;
  const std::uint64_t count = std::min<std::uint64_t>(o.count, w.datagrams.size() - warmup);
  std::cout
      << "open loop, " << (o.stage == Stage::Handler ? "whole handler" : "core only") << ", "
      << what
      << ": items are due at a fixed rate; latency is from the due time to the end of processing.\n"
      << count << " items per run after " << warmup << " warm-up items, " << o.runs
      << " run(s) per rate; each cell is the spread over runs of that run's percentile.\n\n";
  print_sweep_header();
  for (const double rate : o.rates) {
    std::vector<double> p50, p99, max;
    double achieved = 0, late = 0;
    for (unsigned run = 0; run < o.runs; ++run) {
      const OpenLoopResult r = bench_open_loop(w, o.stage, rate, count, warmup);
      const Summary s = summarize(r.corrected);
      p50.push_back(static_cast<double>(s.p50));
      p99.push_back(static_cast<double>(s.p99));
      max.push_back(static_cast<double>(s.max));
      achieved += r.achieved_rate() / o.runs;
      late += 100.0 * static_cast<double>(r.late_starts) / static_cast<double>(r.measured) / o.runs;
    }
    print_spread_row(rate, achieved, late, p50, p99, max);
  }
  std::cout
      << "\n\"late\" is the share of items that started more than 1 us after their due time.\n\n";
}

void run_e2e(const Options& o) {
  Workload w;
  std::string what;
  if (!make_workload_for(o, 1, w, what)) std::exit(2);
  const std::uint64_t warmup = 5000;
  const std::uint64_t count = std::min<std::uint64_t>(o.count, w.datagrams.size() - warmup);
  std::cout << "end to end through loopback multicast, " << what
            << ": due time of the send to the end of processing.\n"
            << count << " items per run, " << o.runs << " run(s) per rate\n\n";
  std::printf("%-10s %-10s %9s %-26s %-26s %-26s\n", "offered", "achieved", "received",
              "p50 (min / median / max)", "p99 (min / median / max)", "max (min / median / max)");
  for (const double rate : o.rates) {
    if (rate > 400000) continue;  // a single sender thread through the kernel does not go faster
    std::vector<double> p50, p99, max;
    double achieved = 0, received_pct = 0;
    for (unsigned run = 0; run < o.runs; ++run) {
      std::string error;
      const auto r = bench_end_to_end(w, rate, count, warmup, error);
      if (!r) {
        std::cerr << "fh_bench: end to end cannot run: " << error << "\n";
        std::exit(2);
      }
      const Summary s = summarize(r->latency);
      p50.push_back(static_cast<double>(s.p50));
      p99.push_back(static_cast<double>(s.p99));
      max.push_back(static_cast<double>(s.max));
      achieved += r->achieved_rate() / o.runs;
      received_pct += 100.0 * static_cast<double>(r->received) /
                      static_cast<double>(std::max<std::uint64_t>(1, r->sent)) / o.runs;
    }
    const Spread a = spread_of(p50);
    const Spread b = spread_of(p99);
    const Spread c = spread_of(max);
    const auto cell = [](const Spread& s) {
      return format_ns(s.min) + " / " + format_ns(s.median) + " / " + format_ns(s.max);
    };
    std::printf("%-10s %-10s %8.2f%% %-26s %-26s %-26s\n", format_rate(rate).c_str(),
                format_rate(achieved).c_str(), received_pct, cell(a).c_str(), cell(b).c_str(),
                cell(c).c_str());
  }
  std::cout << "\n";
}

void run_recovery(const Options& o) {
  const RecoveryResult r = bench_recovery(o.tier, 1, o.scenarios);
  std::cout << "recovery time, simulated time (not CPU time), tier " << fh::sim::tier_name(o.tier)
            << ", " << r.scenarios << " scenarios\n"
            << "  one snapshot loop (period x instruments) = " << format_ns(r.loop_period)
            << "\n\n";
  std::cout << percentile_header();
  std::cout << percentile_row("gap to live", summarize(r.recovery));
  const double loop = static_cast<double>(r.loop_period);
  std::printf("\n  %llu episodes, %llu instruments still recovering at the end of a trace\n",
              static_cast<unsigned long long>(r.episodes),
              static_cast<unsigned long long>(r.unrecovered));
  std::printf("  in snapshot loops: p50 %.2f, p90 %.2f, p99 %.2f, max %.2f\n\n",
              static_cast<double>(r.recovery.value_at_percentile(50)) / loop,
              static_cast<double>(r.recovery.value_at_percentile(90)) / loop,
              static_cast<double>(r.recovery.value_at_percentile(99)) / loop,
              static_cast<double>(r.recovery.max()) / loop);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) return usage();
  const std::string_view command = argv[1];
  Options o;
  for (int i = 2; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const auto value = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
    const char* v = nullptr;
    if (arg == "--events") {
      if ((v = value()) == nullptr) return usage();
      o.events = static_cast<std::size_t>(std::strtoull(v, nullptr, 10));
    } else if (arg == "--repeats") {
      if ((v = value()) == nullptr) return usage();
      o.repeats = static_cast<unsigned>(std::strtoul(v, nullptr, 10));
    } else if (arg == "--pack") {
      if ((v = value()) == nullptr) return usage();
      o.pack = static_cast<unsigned>(std::strtoul(v, nullptr, 10));
    } else if (arg == "--stage") {
      if ((v = value()) == nullptr) return usage();
      const std::string_view s = v;
      if (s == "handler")
        o.stage = Stage::Handler;
      else if (s == "core")
        o.stage = Stage::Core;
      else
        return usage();
    } else if (arg == "--rates") {
      if ((v = value()) == nullptr || !parse_list(v, o.rates)) return usage();
    } else if (arg == "--count") {
      if ((v = value()) == nullptr) return usage();
      o.count = std::strtoull(v, nullptr, 10);
    } else if (arg == "--runs") {
      if ((v = value()) == nullptr) return usage();
      o.runs = static_cast<unsigned>(std::strtoul(v, nullptr, 10));
    } else if (arg == "--tier") {
      if ((v = value()) == nullptr) return usage();
      bool found = false;
      for (unsigned t = 0; t < fh::sim::kTierCount; ++t) {
        if (fh::sim::tier_name(static_cast<fh::sim::Tier>(t)) == v) {
          o.tier = static_cast<fh::sim::Tier>(t);
          found = true;
        }
      }
      if (!found) return usage();
    } else if (arg == "--scenarios") {
      if ((v = value()) == nullptr) return usage();
      o.scenarios = std::strtoull(v, nullptr, 10);
    } else if (arg == "--pin") {
      if ((v = value()) == nullptr) return usage();
      o.pin = std::atoi(v);
    } else if (arg == "--file") {
      if ((v = value()) == nullptr) return usage();
      o.file = v;
    } else if (arg == "--limit") {
      if ((v = value()) == nullptr) return usage();
      o.load.limit = std::strtoull(v, nullptr, 10);
    } else if (arg == "--instruments") {
      if ((v = value()) == nullptr) return usage();
      std::stringstream list(v);
      std::string item;
      while (std::getline(list, item, ',')) {
        const unsigned long id = std::strtoul(item.c_str(), nullptr, 10);
        if (id == 0 || id > 65535) return usage();
        o.load.only.insert(static_cast<fh::InstrumentId>(id));
      }
    } else {
      return usage();
    }
  }
  if (o.events < 20000 || o.repeats == 0 || o.pack == 0 || o.count == 0 || o.runs == 0 ||
      o.scenarios == 0) {
    return usage();
  }

  if (o.pin >= 0 && !pin_to_cpu(o.pin))
    std::cerr << "fh_bench: could not pin to cpu " << o.pin << "; continuing unpinned\n";
  std::cout << render(collect_environment()) << "\n";

  if (command == "env") return 0;
  if (command == "closed") {
    run_closed(o);
  } else if (command == "sweep") {
    run_sweep(o);
  } else if (command == "e2e") {
    run_e2e(o);
  } else if (command == "recovery") {
    run_recovery(o);
  } else if (command == "all") {
    run_closed(o);
    run_sweep(o);
    run_e2e(o);
    run_recovery(o);
  } else {
    return usage();
  }
  return 0;
}
