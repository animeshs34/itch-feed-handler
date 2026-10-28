#include "fh/net/udp.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <utility>

namespace fh::net {
namespace {

std::string describe(const char* what) { return std::string(what) + ": " + std::strerror(errno); }

// Fills `out` from dotted text. Returns false for anything that is not an IPv4 address.
bool parse_address(const std::string& text, in_addr& out) {
  return inet_pton(AF_INET, text.c_str(), &out) == 1;
}

bool is_multicast(const in_addr& address) {
  return (ntohl(address.s_addr) & 0xF0000000U) == 0xE0000000U;  // 224.0.0.0/4
}

}  // namespace

Fd& Fd::operator=(Fd&& other) noexcept {
  if (this != &other) {
    if (fd_ >= 0) ::close(fd_);
    fd_ = other.release();
  }
  return *this;
}

Fd::~Fd() {
  if (fd_ >= 0) ::close(fd_);
}

int Fd::release() noexcept {
  const int fd = fd_;
  fd_ = -1;
  return fd;
}

std::optional<UdpReceiver> UdpReceiver::open(const Endpoint& endpoint, std::string& error,
                                             int receive_buffer_bytes) {
  in_addr group{};
  in_addr iface{};
  if (!parse_address(endpoint.group, group) || !is_multicast(group)) {
    error = "not a multicast group address: " + endpoint.group;
    return std::nullopt;
  }
  if (!parse_address(endpoint.interface_address, iface)) {
    error = "not an IPv4 interface address: " + endpoint.interface_address;
    return std::nullopt;
  }

  Fd fd(::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
  if (!fd.valid()) {
    error = describe("socket");
    return std::nullopt;
  }
  // Several receivers (other processes, a test and a tool) may listen to one group.
  const int on = 1;
  if (::setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) != 0) {
    error = describe("SO_REUSEADDR");
    return std::nullopt;
  }
  // The kernel may give less than asked; that is not an error.
  (void)::setsockopt(fd.get(), SOL_SOCKET, SO_RCVBUF, &receive_buffer_bytes,
                     sizeof receive_buffer_bytes);

  // Bind to the group address itself, so only this group's datagrams arrive even when other
  // groups use the same port.
  sockaddr_in local{};
  local.sin_family = AF_INET;
  local.sin_port = htons(endpoint.port);
  local.sin_addr = group;
  if (::bind(fd.get(), reinterpret_cast<const sockaddr*>(&local), sizeof local) != 0) {
    error = describe("bind");
    return std::nullopt;
  }

  ip_mreq request{};
  request.imr_multiaddr = group;
  request.imr_interface = iface;
  if (::setsockopt(fd.get(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &request, sizeof request) != 0) {
    error = describe("IP_ADD_MEMBERSHIP");
    return std::nullopt;
  }
  return UdpReceiver(std::move(fd));
}

ReceiveStatus UdpReceiver::receive(std::span<std::uint8_t> buffer, std::size_t& size) noexcept {
  size = 0;
  // MSG_TRUNC makes the kernel report the real length, so a datagram that does not fit is
  // noticed instead of silently cut.
  const ssize_t n = ::recv(fd_.get(), buffer.data(), buffer.size(), MSG_DONTWAIT | MSG_TRUNC);
  if (n < 0) {
    return (errno == EAGAIN || errno == EWOULDBLOCK) ? ReceiveStatus::Empty : ReceiveStatus::Error;
  }
  if (static_cast<std::size_t>(n) > buffer.size()) return ReceiveStatus::Truncated;
  size = static_cast<std::size_t>(n);
  return ReceiveStatus::Datagram;
}

std::optional<UdpSender> UdpSender::open(const Endpoint& endpoint, std::string& error) {
  in_addr group{};
  in_addr iface{};
  if (!parse_address(endpoint.group, group) || !is_multicast(group)) {
    error = "not a multicast group address: " + endpoint.group;
    return std::nullopt;
  }
  if (!parse_address(endpoint.interface_address, iface)) {
    error = "not an IPv4 interface address: " + endpoint.interface_address;
    return std::nullopt;
  }
  Fd fd(::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0));
  if (!fd.valid()) {
    error = describe("socket");
    return std::nullopt;
  }
  if (::setsockopt(fd.get(), IPPROTO_IP, IP_MULTICAST_IF, &iface, sizeof iface) != 0) {
    error = describe("IP_MULTICAST_IF");
    return std::nullopt;
  }
  // Without loop-back, a receiver on the same machine would never hear the sender.
  const unsigned char loop = 1;
  if (::setsockopt(fd.get(), IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof loop) != 0) {
    error = describe("IP_MULTICAST_LOOP");
    return std::nullopt;
  }
  const unsigned char ttl = 1;  // never leaves the local network
  if (::setsockopt(fd.get(), IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl) != 0) {
    error = describe("IP_MULTICAST_TTL");
    return std::nullopt;
  }
  return UdpSender(std::move(fd), group.s_addr, htons(endpoint.port));
}

bool UdpSender::send(std::span<const std::uint8_t> datagram) noexcept {
  sockaddr_in to{};
  to.sin_family = AF_INET;
  to.sin_port = port_be_;
  to.sin_addr.s_addr = group_address_be_;
  const ssize_t n = ::sendto(fd_.get(), datagram.data(), datagram.size(), 0,
                             reinterpret_cast<const sockaddr*>(&to), sizeof to);
  return n == static_cast<ssize_t>(datagram.size());
}

}  // namespace fh::net
