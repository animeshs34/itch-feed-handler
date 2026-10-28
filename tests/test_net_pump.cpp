#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <thread>

#include "fh/net/clock.hpp"
#include "fh/net/pump.hpp"
#include "net_ports.hpp"

TEST_CASE("the pump hands every datagram to the callback with its source and a time",
          "[net][pump]") {
  const auto e0 = fhtest::fresh_endpoint();
  const auto e1 = fhtest::fresh_endpoint();
  std::string error;
  auto r0 = fh::net::UdpReceiver::open(e0, error);
  auto r1 = fh::net::UdpReceiver::open(e1, error);
  auto s0 = fh::net::UdpSender::open(e0, error);
  auto s1 = fh::net::UdpSender::open(e1, error);
  REQUIRE(r0.has_value());
  REQUIRE(r1.has_value());
  REQUIRE(s0.has_value());
  REQUIRE(s1.has_value());

  const fh::net::MonotonicClock clock;
  fh::net::DatagramPump pump({&*r0, &*r1}, [&clock] { return clock.now(); }, fh::kMillisecond);

  struct Got {
    std::size_t source;
    fh::Nanos t;
    std::vector<std::uint8_t> bytes;
  };
  std::vector<Got> got;
  std::atomic<bool> done{false};

  std::thread sender([&] {
    for (std::uint8_t i = 0; i < 10; ++i) {
      REQUIRE(((i % 2 == 0) ? *s0 : *s1).send(std::vector<std::uint8_t>{i}));
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    done = true;
  });
  pump.run(
      [&](std::size_t source, fh::Nanos t, std::span<const std::uint8_t> bytes) {
        got.push_back(Got{source, t, std::vector<std::uint8_t>(bytes.begin(), bytes.end())});
      },
      [](fh::Nanos) {}, [&] { return done.load(); });
  sender.join();

  REQUIRE(got.size() == 10);
  for (std::uint8_t i = 0; i < 10; ++i) {
    REQUIRE(got[i].source == i % 2);
    REQUIRE(got[i].bytes == std::vector<std::uint8_t>{i});
    if (i > 0) REQUIRE(got[i].t >= got[i - 1].t);
  }
  REQUIRE(pump.stats().datagrams[0] == 5);
  REQUIRE(pump.stats().datagrams[1] == 5);
  REQUIRE(pump.stats().bytes[0] == 5);
  REQUIRE(pump.stats().largest_datagram == 1);
}

TEST_CASE("the pump ticks about once per period even when nothing arrives", "[net][pump]") {
  std::string error;
  auto rx = fh::net::UdpReceiver::open(fhtest::fresh_endpoint(), error);
  REQUIRE(rx.has_value());
  const fh::net::MonotonicClock clock;
  fh::net::DatagramPump pump({&*rx}, [&clock] { return clock.now(); }, 5 * fh::kMillisecond);

  std::vector<fh::Nanos> ticks;
  pump.run([](std::size_t, fh::Nanos, std::span<const std::uint8_t>) {},
           [&](fh::Nanos t) { ticks.push_back(t); },
           [&] { return clock.now() > 100 * fh::kMillisecond; });

  REQUIRE(ticks.size() >= 10);  // 100 ms at 5 ms is 20; a loaded machine may lose some
  REQUIRE(ticks.size() <= 21);
  for (std::size_t i = 1; i < ticks.size(); ++i) {
    REQUIRE(ticks[i] - ticks[i - 1] >= 5 * fh::kMillisecond);  // never early
  }
  REQUIRE(pump.stats().ticks == ticks.size());
}

TEST_CASE("the pump stops at once when asked before it starts", "[net][pump]") {
  std::string error;
  auto rx = fh::net::UdpReceiver::open(fhtest::fresh_endpoint(), error);
  const fh::net::MonotonicClock clock;
  fh::net::DatagramPump pump({&*rx}, [&clock] { return clock.now(); }, fh::kMillisecond);
  std::size_t calls = 0;
  pump.run([&](std::size_t, fh::Nanos, std::span<const std::uint8_t>) { ++calls; },
           [&](fh::Nanos) { ++calls; }, [] { return true; });
  REQUIRE(calls == 0);
  REQUIRE(pump.stats().wakeups == 0);
}

TEST_CASE("a fake clock drives the ticks, so the loop can be tested without real waiting",
          "[net][pump]") {
  std::string error;
  auto rx = fh::net::UdpReceiver::open(fhtest::fresh_endpoint(), error);
  fh::Nanos fake = 0;
  // Each reading of the clock advances it by one millisecond.
  fh::net::DatagramPump pump(
      {&*rx}, [&fake] { return fake += fh::kMillisecond; }, 10 * fh::kMillisecond);
  std::size_t ticks = 0;
  pump.run([](std::size_t, fh::Nanos, std::span<const std::uint8_t>) {},
           [&](fh::Nanos) { ++ticks; }, [&] { return ticks >= 3; });
  REQUIRE(ticks == 3);
}
