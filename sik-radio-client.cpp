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
#define DEFAULT_PORT_HTTP 80
#define DEFAULT_PORT_HTTPS 443

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
