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

// Following two functions are copied from MIMUW course.
// `read_port` is overloaded to use with `cpp` strings.

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

