#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

#include "SikClient.h"
#include "common.h"
#include "err.h"

#define MIN_TIMEOUT 100
#define MAX_TIMEOUT 100000
#define MIN_VERBOSITY 0
#define MAX_VERBOSITY 4

// Get and validate program arguments. Last argument of each type is taken into
// consideration. Validate only the last occurences.
ClientConfig handle_arguments(int argc, char *argv[]) {
  ClientConfig config;
  int opt;
  bool got_4 = false, got_6 = false, got_url = false;

  // Store last raw values.
  const char *url_arg = nullptr;
  const char *timeout_arg = nullptr;
  const char *verbosity_arg = nullptr;
  bool q_was_last = false;

  while ((opt = getopt(argc, argv, "u:mt:46v:q")) != -1) {
    switch (opt) {
      case 'u':
        url_arg = optarg;
        got_url = true;
        break;
      case 'm':
        config.multiplexing = true;
        break;
      case 't':
        timeout_arg = optarg;
        break;
      case '4':
        got_4 = true;
        config.ip_version = got_6 ? IpVersion::AUTO : IpVersion::IPV4;
        break;
      case '6':
        got_6 = true;
        config.ip_version = got_4 ? IpVersion::AUTO : IpVersion::IPV6;
        break;
      case 'v':
        verbosity_arg = optarg;
        q_was_last = false;
        break;
      case 'q':
        verbosity_arg = nullptr;
        q_was_last = true;
        break;
      default:
        fatal(2,
              "Usage: %s -u <url> [-m] [-t <timeout>] [-4] [-6]"
              " [-v <verbosity>] [-q]",
              argv[0]);
    }
  }

  if (!got_url) {
    fatal(2,
          "Usage: %s -u <url> [-m] [-t <timeout>] [-4] [-6]"
          " [-v <verbosity>] [-q]",
          argv[0]);
  }

  config.url = parse_url(url_arg, 2);

  if (timeout_arg != nullptr) {
    config.timeout = (int)ulong_from_str(MIN_TIMEOUT, MAX_TIMEOUT,
                                         "client timeout", timeout_arg, 2);
  }

  if (q_was_last) {
    config.verbosity = 0;
  } else if (verbosity_arg != nullptr) {
    config.verbosity = (uint8_t)ulong_from_str(
        MIN_VERBOSITY, MAX_VERBOSITY, "verbosity level", verbosity_arg, 2);
  }

  return config;
}

int main(int argc, char *argv[]) {
  // Ignore SIGPIPE so writes to a closed pipe return -1 instead of killing the
  // process.
  ClientConfig config = handle_arguments(argc, argv);
  install_signal_handler(SIGPIPE, SIG_IGN, 0 /* flags = 0 */, config.verbosity);

  SikClient client = SikClient(config);
  client.run();

  return 0;
}
