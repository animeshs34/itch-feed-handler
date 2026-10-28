#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

#include "fh/net/udp.hpp"
#include "net_ports.hpp"

namespace {

// Waits up to a second for one datagram.
fh::net::ReceiveStatus receive_within(fh::net::UdpReceiver& rx, std::vector<std::uint8_t>& out) {
  std::vector<std::uint8_t> buffer(2048);
  for (int i = 0; i < 1000; ++i) {
    std::size_t size = 0;
    const auto status = rx.receive(buffer, size);
    if (status != fh::net::ReceiveStatus::Empty) {
      out.assign(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(size));
      return status;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return fh::net::ReceiveStatus::Empty;
}

}  // namespace

TEST_CASE("a datagram sent to a group is received by a member of it", "[net][udp]") {
  const auto endpoint = fhtest::fresh_endpoint();
  std::string error;
  auto rx = fh::net::UdpReceiver::open(endpoint, error);
  INFO(error);
  REQUIRE(rx.has_value());
  auto tx = fh::net::UdpSender::open(endpoint, error);
  REQUIRE(tx.has_value());

  const std::vector<std::uint8_t> message = {1, 2, 3, 4, 5};
  REQUIRE(tx->send(message));
  std::vector<std::uint8_t> got;
  REQUIRE(receive_within(*rx, got) == fh::net::ReceiveStatus::Datagram);
  REQUIRE(got == message);
}

TEST_CASE("datagrams keep their boundaries and their order", "[net][udp]") {
  const auto endpoint = fhtest::fresh_endpoint();
  std::string error;
  auto rx = fh::net::UdpReceiver::open(endpoint, error);
  auto tx = fh::net::UdpSender::open(endpoint, error);
  REQUIRE(rx.has_value());
  REQUIRE(tx.has_value());
  for (std::uint8_t i = 1; i <= 20; ++i) {
    REQUIRE(tx->send(std::vector<std::uint8_t>(i, i)));
  }
  for (std::uint8_t i = 1; i <= 20; ++i) {
    std::vector<std::uint8_t> got;
    REQUIRE(receive_within(*rx, got) == fh::net::ReceiveStatus::Datagram);
    REQUIRE(got == std::vector<std::uint8_t>(i, i));
  }
}

TEST_CASE("an empty socket says Empty and does not block", "[net][udp]") {
  std::string error;
  auto rx = fh::net::UdpReceiver::open(fhtest::fresh_endpoint(), error);
  REQUIRE(rx.has_value());
  std::vector<std::uint8_t> buffer(100);
  std::size_t size = 99;
  REQUIRE(rx->receive(buffer, size) == fh::net::ReceiveStatus::Empty);
  REQUIRE(size == 0);
}

TEST_CASE("a datagram too big for the buffer is reported and discarded", "[net][udp]") {
  const auto endpoint = fhtest::fresh_endpoint();
  std::string error;
  auto rx = fh::net::UdpReceiver::open(endpoint, error);
  auto tx = fh::net::UdpSender::open(endpoint, error);
  REQUIRE(rx.has_value());
  REQUIRE(tx.has_value());
  REQUIRE(tx->send(std::vector<std::uint8_t>(500, 7)));
  std::vector<std::uint8_t> small(100);
  std::size_t size = 0;
  fh::net::ReceiveStatus status = fh::net::ReceiveStatus::Empty;
  for (int i = 0; i < 1000 && status == fh::net::ReceiveStatus::Empty; ++i) {
    status = rx->receive(small, size);
    if (status == fh::net::ReceiveStatus::Empty)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(status == fh::net::ReceiveStatus::Truncated);
  // And it is gone: the next read finds nothing.
  REQUIRE(rx->receive(small, size) == fh::net::ReceiveStatus::Empty);
}

TEST_CASE("two receivers of one group both get every datagram", "[net][udp]") {
  const auto endpoint = fhtest::fresh_endpoint();
  std::string error;
  auto a = fh::net::UdpReceiver::open(endpoint, error);
  auto b = fh::net::UdpReceiver::open(endpoint, error);
  auto tx = fh::net::UdpSender::open(endpoint, error);
  INFO(error);
  REQUIRE(a.has_value());
  REQUIRE(b.has_value());
  REQUIRE(tx.has_value());
  REQUIRE(tx->send(std::vector<std::uint8_t>{9, 9}));
  std::vector<std::uint8_t> ga, gb;
  REQUIRE(receive_within(*a, ga) == fh::net::ReceiveStatus::Datagram);
  REQUIRE(receive_within(*b, gb) == fh::net::ReceiveStatus::Datagram);
  REQUIRE(ga == gb);
}

TEST_CASE("a group does not hear another group on the same port", "[net][udp]") {
  auto one = fhtest::fresh_endpoint("239.255.42.1");
  auto two = one;
  two.group = "239.255.42.2";
  std::string error;
  auto rx = fh::net::UdpReceiver::open(one, error);
  auto tx = fh::net::UdpSender::open(two, error);
  REQUIRE(rx.has_value());
  REQUIRE(tx.has_value());
  REQUIRE(tx->send(std::vector<std::uint8_t>{1}));
  std::vector<std::uint8_t> got;
  REQUIRE(receive_within(*rx, got) == fh::net::ReceiveStatus::Empty);
}

TEST_CASE("bad addresses are refused with a reason", "[net][udp]") {
  fh::net::Endpoint e = fhtest::fresh_endpoint();
  std::string error;
  e.group = "10.0.0.1";  // not multicast
  REQUIRE_FALSE(fh::net::UdpReceiver::open(e, error).has_value());
  REQUIRE(error.find("multicast") != std::string::npos);
  REQUIRE_FALSE(fh::net::UdpSender::open(e, error).has_value());
  e.group = "not an address";
  REQUIRE_FALSE(fh::net::UdpReceiver::open(e, error).has_value());
  e = fhtest::fresh_endpoint();
  e.interface_address = "nonsense";
  REQUIRE_FALSE(fh::net::UdpReceiver::open(e, error).has_value());
  REQUIRE(error.find("interface") != std::string::npos);
}

TEST_CASE("a moved descriptor closes once and the moved-from one is empty", "[net][udp]") {
  fh::net::Fd a;
  REQUIRE_FALSE(a.valid());
  std::string error;
  auto rx = fh::net::UdpReceiver::open(fhtest::fresh_endpoint(), error);
  REQUIRE(rx.has_value());
  const int raw = rx->fd();
  fh::net::UdpReceiver moved = std::move(*rx);
  REQUIRE(moved.fd() == raw);
  fh::net::Fd b(::dup(raw));
  REQUIRE(b.valid());
  fh::net::Fd c = std::move(b);
  REQUIRE_FALSE(
      b.valid());  // NOLINT(bugprone-use-after-move): the test is about the moved-from state
  REQUIRE(c.valid());
  REQUIRE(c.release() >= 0);
  REQUIRE_FALSE(c.valid());
}
