#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "err.h"

// Following three functions are copied from MIMUW course.
// `read_port`, `get_server_address` are overloaded to to use with cpp stings.

uint16_t read_port(const std::string &str) { return read_port(str.c_str()); }

uint16_t read_port(char const *str) {
  char *endptr;
  errno = 0;
  unsigned long port = strtoul(str, &endptr, 10);
  if (errno != 0 || *endptr != 0 || port > UINT16_MAX) {
    fatal("%s is not a valid port number", str);
  }
  return (uint16_t)port;
}

void install_signal_handler(int signal, void (*handler)(int), int flags) {
  struct sigaction action;
  sigset_t block_mask;

  sigemptyset(&block_mask);
  action.sa_handler = handler;
  action.sa_mask = block_mask;
  action.sa_flags = flags;

  if (sigaction(signal, &action, NULL) < 0) {
    syserr("sigaction");
  }
}

struct sockaddr_in get_server_address(std::string const &host, uint16_t port) {
  return get_server_address(host.c_str(), port);
}

struct sockaddr_in get_server_address(char const *host, uint16_t port) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;

  struct addrinfo *address_result;
  int errcode = getaddrinfo(host, NULL, &hints, &address_result);
  if (errcode != 0) {
    fatal("getaddrinfo: %s", gai_strerror(errcode));
  }

  struct sockaddr_in send_address;
  memset(&send_address, 0, sizeof(send_address));
  send_address.sin_family = AF_INET;
  send_address.sin_addr.s_addr =
      ((struct sockaddr_in *)(address_result->ai_addr))->sin_addr.s_addr;
  send_address.sin_port = htons(port);

  freeaddrinfo(address_result);

  return send_address;
}
