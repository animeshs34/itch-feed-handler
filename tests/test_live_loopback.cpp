#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <thread>

#include "fh/live/live.hpp"
#include "fh/sim/checker.hpp"
#include "fh/sim/scenario.hpp"
#include "net_ports.hpp"

namespace {

// Real sockets, one process: a sender thread plays the trace out on three multicast groups
// of the loopback interface and the main thread runs the live handler on them.
//
// The day is slowed down by `scale` and the handler's windows are scaled to match, so
// that the few hundred microseconds of scheduling jitter of an ordinary machine are small
// next to the windows the handler measures.
struct LiveRun {
  fh::live::LiveResult result;
  fh::live::SendStats sent;
};

LiveRun play(const fh::sim::Trace& original, std::int64_t scale,
             const std::string& interface_address = "127.0.0.1") {
  fh::live::LiveConfig config;
  const auto first_port = fhtest::fresh_port();
  (void)fhtest::fresh_port();
  (void)fhtest::fresh_port();
  config.groups = fh::live::groups_on(first_port, "239.255.42.1", interface_address);
  config.core = fh::live::scale_core(original.core, scale);
  config.tick_period = 200 * fh::kMicrosecond * scale;

  fh::live::SendConfig send;
  send.groups = config.groups;
  send.scale = scale;

  std::atomic<bool> done{false};
  LiveRun run;
  std::string error;
  std::thread sender([&] {
    // Give the receiver a moment to join its groups.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string send_error;
    const auto stats = fh::live::send_trace(original, send, send_error);
    INFO(send_error);
    REQUIRE(stats.has_value());
    run.sent = *stats;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));  // let the last datagrams arrive
    done = true;
  });
  auto result = fh::live::run_live(config, [&] { return done.load(); }, error);
  sender.join();
  INFO(error);
  REQUIRE(result.has_value());
  run.result = std::move(*result);
  return run;
}

}  // namespace

TEST_CASE("a clean day sent through real sockets ends with every book right", "[live]") {
  const auto original = fh::sim::make_trace(3, fh::sim::scenario_config(fh::sim::Tier::Clean));
  const LiveRun run = play(original, 10);

  REQUIRE(run.sent.failed == 0);
  REQUIRE(run.result.stats.truncated == 0);
  REQUIRE(run.result.stats.receive_errors == 0);

  // Judge the recorded trace against the answer key the sender had.
  fh::sim::Trace judged = run.result.trace;
  judged.truth = original.truth;
  const fh::sim::Verdict verdict = fh::sim::run_trace(judged);
  for (const auto& f : verdict.safety_failures) UNSCOPED_INFO(f);
  for (const auto& f : verdict.liveness_failures) UNSCOPED_INFO(f);
  REQUIRE(verdict.ok());
  REQUIRE(run.result.final_state.has_value());
}

TEST_CASE("a recorded live run replays to exactly the same outputs", "[live]") {
  const auto original = fh::sim::make_trace(5, fh::sim::scenario_config(fh::sim::Tier::Mild));
  const LiveRun run = play(original, 10);

  std::vector<fh::Output> replayed;
  const fh::sim::Verdict again = fh::sim::run_trace(run.result.trace, &replayed);
  (void)again;
  REQUIRE(replayed.size() == run.result.outputs.size());
  REQUIRE(replayed == run.result.outputs);
}

TEST_CASE("every datagram the sender sent reached the handler", "[live]") {
  const auto original = fh::sim::make_trace(7, fh::sim::scenario_config(fh::sim::Tier::Clean));
  const LiveRun run = play(original, 10);
  std::uint64_t main_and_snapshot = 0;
  for (const auto& e : original.entries)
    main_and_snapshot += e.kind != fh::sim::EntryKind::Tick ? 1U : 0U;
  REQUIRE(run.sent.sent == main_and_snapshot);
  std::uint64_t received = 0;
  for (const auto n : run.result.stats.datagrams) received += n;
  REQUIRE(received == main_and_snapshot);  // loopback with a large buffer drops nothing
}

TEST_CASE("a faulty day through real sockets is still never wrong", "[live]") {
  const auto original = fh::sim::make_trace(11, fh::sim::scenario_config(fh::sim::Tier::Heavy));
  const LiveRun run = play(original, 10);
  fh::sim::Trace judged = run.result.trace;
  judged.truth = original.truth;
  const fh::sim::Verdict verdict = fh::sim::run_trace(judged);
  for (const auto& f : verdict.safety_failures) UNSCOPED_INFO(f);
  for (const auto& f : verdict.liveness_failures) UNSCOPED_INFO(f);
  REQUIRE(verdict.safe());
}

TEST_CASE("run_live refuses a group it cannot join", "[live]") {
  fh::live::LiveConfig config;
  config.groups = fh::live::groups_on(fhtest::fresh_port());
  config.groups.line_b.group = "10.1.1.1";  // not multicast
  std::string error;
  REQUIRE_FALSE(fh::live::run_live(config, [] { return true; }, error).has_value());
  REQUIRE(error.find("multicast") != std::string::npos);
}

TEST_CASE("scaling the core settings scales both windows and nothing else", "[live]") {
  fh::sim::CoreParams p;
  const auto s = fh::live::scale_core(p, 10);
  REQUIRE(s.skew_window == 10 * p.skew_window);
  REQUIRE(s.silent_window == 10 * p.silent_window);
  REQUIRE(s.buffer_capacity == p.buffer_capacity);
}

TEST_CASE("the three groups are on consecutive ports", "[live]") {
  const auto g = fh::live::groups_on(30000, "239.1.2.3", "127.0.0.1");
  REQUIRE(g.line_a.port == 30000);
  REQUIRE(g.line_b.port == 30001);
  REQUIRE(g.snapshots.port == 30002);
  REQUIRE(g.line_a.group == "239.1.2.3");
}

TEST_CASE("a quiet listener records only ticks, and they are stamped in order", "[live]") {
  fh::live::LiveConfig config;
  config.groups = fh::live::groups_on(fhtest::fresh_port());
  (void)fhtest::fresh_port();
  (void)fhtest::fresh_port();
  config.tick_period = 2 * fh::kMillisecond;
  const auto start = std::chrono::steady_clock::now();
  std::string error;
  const auto result = fh::live::run_live(
      config,
      [&] { return std::chrono::steady_clock::now() - start > std::chrono::milliseconds(60); },
      error);
  REQUIRE(result.has_value());
  REQUIRE(result->trace.entries.size() >= 10);
  for (const auto& e : result->trace.entries) REQUIRE(e.kind == fh::sim::EntryKind::Tick);
  for (std::size_t i = 1; i < result->trace.entries.size(); ++i) {
    REQUIRE(result->trace.entries[i].t > result->trace.entries[i - 1].t);
  }
  // Silence is itself news: the only outputs are feed-health changes.
  for (const auto& o : result->outputs) REQUIRE(std::holds_alternative<fh::FeedHealthChange>(o));
  REQUIRE_FALSE(result->final_state.has_value());  // nothing was ever seen
}

TEST_CASE("the same day over a real network interface, if the machine has one", "[live]") {
  const auto address = fhtest::multicast_interface_address();
  if (!address) SKIP("no multicast-capable interface other than the loopback");
  const auto original = fh::sim::make_trace(3, fh::sim::scenario_config(fh::sim::Tier::Clean));
  const LiveRun run = play(original, 10, *address);
  fh::sim::Trace judged = run.result.trace;
  judged.truth = original.truth;
  REQUIRE(fh::sim::run_trace(judged).ok());
}
