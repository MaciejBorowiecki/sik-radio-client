#include <cstdlib>
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>

#include "SikClient.h"
#include "common.h"
#include "err.h"

#define MIN_TIMEOUT 100
#define MAX_TIMEOUT 100000
#define MIN_VERBOSITY 0
#define MAX_VERBOSITY 4

// Get and validate program arguments. Last argument of each type is taken into
// consideration.
ClientConfig handle_arguments(int argc, char *argv[]) {
  ClientConfig config;

  int opt;

  // Helper variables to check which ip version is forced.
  bool got_4 = false, got_6 = false;

  // Helper variable to determine whether url was given.
  bool got_url = false;

  // TODO: może weryfikowac tylko ostatnie wystąpienie parametru? albo tylko z
  // fatal wtedy weryfikować.
  while ((opt = getopt(argc, argv, "u:mt:46v:q")) != -1) {
    switch (opt) {
      case 'u':
        config.url = parse_url(optarg);
        got_url = true;
        break;
      case 'm':
        config.multiplexing = true;
        break;
      case 't': {
        config.timeout = (int)ulong_from_str(MIN_TIMEOUT, MAX_TIMEOUT,
                                             "client timeout", optarg);
        break;
      }
      case '4': {
        got_4 = true;
        if (!got_6) {
          config.ip_version = IpVersion::IPV4;
        } else {
          config.ip_version = IpVersion::AUTO;
        }
        break;
      }
      case '6': {
        got_6 = true;
        if (!got_4) {
          config.ip_version = IpVersion::IPV6;
        } else {
          config.ip_version = IpVersion::AUTO;
        }
        break;
      }
      case 'v': {
        config.verbosity = (uint8_t)ulong_from_str(MIN_VERBOSITY, MAX_VERBOSITY,
                                                   "verbosity level", optarg);
        break;
      }
      case 'q':
        config.verbosity = 0;
        break;
      default:
        fatal("Usage: %s -u <url> [-m] [-t] <timeout> [-4] [-6] [-v] "
              "<verbosity> [-q]");
    }
  }
  if (!got_url) {
    fatal("Usage: %s -u <url> [-m] [-t] <timeout> [-4] [-6] [-v] "
          "<verbosity> [-q]");
  }

  return config;
}

int main(int argc, char *argv[]) {
  ClientConfig config = handle_arguments(argc, argv);
  
  SikClient client = SikClient(config);
  client.run();

  return 0;
}
