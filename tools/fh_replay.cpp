// fh_replay: replays one saved trace through the Nasdaq handler and prints the verdict.
//
//   fh_replay FILE.fhtr
//
// Exit status: 0 every property held, 1 some property failed, 2 the file could not be read.

#include <iostream>

#include "fh/sim/checker.hpp"
#include "fh/sim/trace_io.hpp"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: fh_replay FILE.fhtr\n";
    return 2;
  }
  const auto trace = fh::sim::read_trace_file(argv[1]);
  if (!trace) {
    std::cerr << "fh_replay: cannot read a trace from " << argv[1] << "\n";
    return 2;
  }
  const fh::sim::Verdict verdict = fh::sim::run_trace(*trace);

  std::cout << "seed " << trace->seed << ", " << trace->truth.messages.size() << " true messages, "
            << trace->entries.size() << " entries\n";
  std::cout << "updates published: " << verdict.updates << "\n";
  std::cout << "gaps declared: " << verdict.counters.gaps << "\n";
  for (const std::string& f : verdict.safety_failures) std::cout << "FAIL (safety) " << f << "\n";
  for (const std::string& f : verdict.liveness_failures)
    std::cout << "FAIL (liveness) " << f << "\n";
  std::cout << "output digest: " << std::hex << verdict.output_digest << "\n";
  std::cout << "final checksum: " << std::hex << verdict.final_checksum << std::dec << "\n";
  std::cout << (verdict.ok() ? "result: PASS\n" : "result: FAIL\n");
  return verdict.ok() ? 0 : 1;
}
