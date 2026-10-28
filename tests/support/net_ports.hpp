#ifndef FH_TESTS_SUPPORT_NET_PORTS_HPP_
#define FH_TESTS_SUPPORT_NET_PORTS_HPP_

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>

#include <cstdint>
#include <optional>
#include <string>

#include "fh/net/udp.hpp"

namespace fhtest {

// A port that is unlikely to be in use, different for every process and every call, so that
// two test runs at the same time (or a test that leaves a socket in TIME_WAIT) do not meet.
inline std::uint16_t fresh_port() {
  static std::uint16_t next =
      static_cast<std::uint16_t>(20000 + (static_cast<unsigned>(::getpid()) * 7U) % 20000U);
  next = static_cast<std::uint16_t>(next + 1);
  return next;
}

inline fh::net::Endpoint fresh_endpoint(const char* group = "239.255.42.1") {
  fh::net::Endpoint e;
  e.group = group;
  e.port = fresh_port();
  return e;
}

// The IPv4 address of the first interface that is up, can multicast and is not the loopback,
// or nothing if the machine has none (a container without a network, for instance).
inline std::optional<std::string> multicast_interface_address() {
  ifaddrs* list = nullptr;
  if (::getifaddrs(&list) != 0) return std::nullopt;
  std::optional<std::string> found;
  for (const ifaddrs* p = list; p != nullptr && !found; p = p->ifa_next) {
    if (p->ifa_addr == nullptr || p->ifa_addr->sa_family != AF_INET) continue;
    const unsigned flags = p->ifa_flags;
    if ((flags & IFF_UP) == 0 || (flags & IFF_MULTICAST) == 0 || (flags & IFF_LOOPBACK) != 0)
      continue;
    char text[INET_ADDRSTRLEN];
    const auto* in = reinterpret_cast<const sockaddr_in*>(p->ifa_addr);
    if (::inet_ntop(AF_INET, &in->sin_addr, text, sizeof text) != nullptr)
      found = std::string(text);
  }
  ::freeifaddrs(list);
  return found;
}

}  // namespace fhtest

#endif  // FH_TESTS_SUPPORT_NET_PORTS_HPP_
