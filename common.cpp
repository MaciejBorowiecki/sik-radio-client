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
// `read_port` and writen are overloaded to use with `cpp` strings.

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

// Write n bytes to a descriptor.
ssize_t writen(int fd, const void *vptr, size_t n) {
    ssize_t nleft = n;
    ssize_t nwritten;
    
    const char *ptr = static_cast<const char*>(vptr); 

    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR) {
                nwritten = 0; 
            } else {
                return -1;   
            }
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}

ssize_t writen(int fd, const std::string& str) {
    return writen(fd, str.c_str(), str.size());
}

unsigned long ulong_from_str(int min_val, int max_val, const char* num_type,
                             const char *str) {
  char *endptr;
  unsigned long num = strtoul(str, &endptr, 10);
  if (*endptr != '\0' || num < min_val || num > max_val) {
    fatal("%s is not a valid %s number", str, num_type);
  }
  return num;
}

// TODO: może zwracanie konkretniejszego info w fatal, tylko nie może być wtedy
// w std::string lub przeciążyć w err
ParsedUrl parse_url(const std::string &url) {
  ParsedUrl parsed_url;
  int pos;

  // check for :// (first anchor in url)
  if ((pos = url.find("://")) == std::string::npos) {
    fatal("Invalid URL given.");
  }

  std::string prot = url.substr(0, pos);

  if (prot == "http") {
    parsed_url.is_ssl = false;
    parsed_url.port = DEFAULT_PORT_HTTP;
  } else if (prot == "https") {
    parsed_url.is_ssl = true;
    parsed_url.port = DEFAULT_PORT_HTTPS;
  } else {
    fatal("Invalid protocol in given URL.");
  }

  // Start of the next section of the URL is end is after "://" anchor.
  size_t host_start = pos + 3;
  size_t path_start = url.find('/', host_start);
  size_t port_start = url.find(':', host_start);

  // ':' must occur before path starts to represent port.
  bool has_port = (port_start != std::string::npos) &&
                  (path_start == std::string::npos || port_start < path_start);

  if (has_port) {
    parsed_url.host = url.substr(host_start, port_start - host_start);

    if (path_start == std::string::npos) {
      parsed_url.port = read_port(url.substr(port_start + 1));
      parsed_url.path = "/";
    } else {
      parsed_url.port = read_port(
          url.substr(port_start + 1, path_start - (port_start + 1)));
      parsed_url.path = url.substr(path_start);
    }
  } else {
    if (path_start == std::string::npos) {
      parsed_url.host = url.substr(host_start);
      parsed_url.path = "/";
    } else {
      parsed_url.host = url.substr(host_start, path_start - host_start);
      parsed_url.path = url.substr(path_start);
    }
  }

  return parsed_url;
}
