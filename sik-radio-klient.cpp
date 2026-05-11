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

unsigned long ulong_from_str(int min_val, int max_val, const char* num_type,
                             const char *str) {
  char *endptr;
  unsigned long num = strtoul(str, &endptr, 10);
  if (*endptr != '\0' || num < min_val || num > max_val) {
    fatal("%s is not a valid %s number", str, num_type);
  }
  return num;
}

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
        config.url = optarg;
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

  std::cout << "URL (-u):" << config.url << "\n";
  std::cout << "Multiplexing (-m): " << (config.multiplexing ? "Yes" : "No") << "\n";
  std::cout << "Timeout (-t): " << config.timeout << " ms\n";
  std::cout << "Verbosity (-v): " << (int)config.verbosity << "\n";

  std::cout << "IP Version (-4 / -6): ";
  switch (config.ip_version) {
      case IpVersion::IPV4:
          std::cout << "IPv4\n";
          break;
      case IpVersion::IPV6:
          std::cout << "IPv6\n";
          break;
      case IpVersion::AUTO:
          std::cout << "AUTO\n";
          break;
  }
}
