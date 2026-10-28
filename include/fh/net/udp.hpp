#ifndef FH_NET_UDP_HPP_
#define FH_NET_UDP_HPP_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace fh::net {

// Where a multicast group lives: its address, its port, and the local interface that
// carries it, given by that interface's IPv4 address. The default is the loopback
// interface, so nothing leaves the machine.
struct Endpoint {
  std::string group{"239.255.42.1"};
  std::uint16_t port{0};
  std::string interface_address{"127.0.0.1"};
};

// A file descriptor that closes itself. It can be moved and never copied, so exactly one
// owner ever closes it.
class Fd {
 public:
  Fd() noexcept = default;
  explicit Fd(int fd) noexcept : fd_(fd) {}
  Fd(const Fd&) = delete;
  Fd& operator=(const Fd&) = delete;
  Fd(Fd&& other) noexcept : fd_(other.release()) {}
  Fd& operator=(Fd&& other) noexcept;
  ~Fd();

  [[nodiscard]] int get() const noexcept { return fd_; }
  [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }
  // Gives up ownership without closing.
  int release() noexcept;

 private:
  int fd_{-1};
};

enum class ReceiveStatus : std::uint8_t {
  Datagram,   // one datagram was read; `size` is its length
  Empty,      // nothing waiting
  Truncated,  // a datagram was waiting but did not fit the buffer; it was discarded
  Error,      // the socket failed
};

// Receives datagrams of one multicast group without blocking.
class UdpReceiver {
 public:
  // Joins `endpoint` and returns the receiver, or nothing with the reason in `error`.
  // `receive_buffer_bytes` asks the kernel for a larger socket buffer, because a burst
  // that arrives faster than the loop reads is otherwise dropped by the kernel without
  // a word; the kernel may grant less.
  [[nodiscard]] static std::optional<UdpReceiver> open(const Endpoint& endpoint, std::string& error,
                                                       int receive_buffer_bytes = 4 * 1024 * 1024);

  // Reads one datagram into `buffer` if one is waiting.
  [[nodiscard]] ReceiveStatus receive(std::span<std::uint8_t> buffer, std::size_t& size) noexcept;

  [[nodiscard]] int fd() const noexcept { return fd_.get(); }

 private:
  explicit UdpReceiver(Fd fd) noexcept : fd_(std::move(fd)) {}
  Fd fd_;
};

// Sends datagrams to one multicast group.
class UdpSender {
 public:
  [[nodiscard]] static std::optional<UdpSender> open(const Endpoint& endpoint, std::string& error);

  // Sends one datagram. Returns false if the kernel refused it.
  [[nodiscard]] bool send(std::span<const std::uint8_t> datagram) noexcept;

 private:
  UdpSender(Fd fd, std::uint32_t group_address_be, std::uint16_t port_be) noexcept
      : fd_(std::move(fd)), group_address_be_(group_address_be), port_be_(port_be) {}
  Fd fd_;
  std::uint32_t group_address_be_;
  std::uint16_t port_be_;
};

}  // namespace fh::net

#endif  // FH_NET_UDP_HPP_
