#include <cinttypes>
#include <cstring>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "SikClient.h"
#include "common.h"
#include "err.h"

SikClient::SikClient(const ClientConfig &cfg) : config(cfg) {}

SikClient::~SikClient() {}

void SikClient::connect_to_server(const std::string &host, uint16_t port,
                                  IpVersion ip_version) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  if (ip_version == IpVersion::IPV4) {
    hints.ai_family = AF_INET;
  } else if (ip_version == IpVersion::IPV6) {
    hints.ai_family = AF_INET6;
  } else {
    hints.ai_family = AF_UNSPEC;
  }

  struct addrinfo *result, *rp;
  std::string port_str = std::to_string(port);
  int errcode = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
  if (errcode != 0) {
    fatal("getaddrinfo: %s", gai_strerror(errcode));
  }

  socket_fd = -1;
  for (rp = result; rp != nullptr; rp = rp->ai_next) {
    socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (socket_fd == -1) {
      continue;
    }

    if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
    }

    close(socket_fd);
  }

  freeaddrinfo(result);

  if (rp == nullptr) {
    fatal("Could not connect to the server.");
  }
}
