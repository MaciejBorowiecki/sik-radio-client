#ifndef SIK_RADIO_CLIENT_H
#define SIK_RADIO_CLIENT_H

#include <string>
#include <cinttypes>

enum struct IpVersion {
  AUTO,
  IPV4,
  IPV6
};

struct ClientConfig {
  std::string url;
  bool multiplexing = false;
  int timeout = 5000;
  IpVersion ip_version = IpVersion::AUTO;
  uint8_t verbosity = 2;
};

#endif
