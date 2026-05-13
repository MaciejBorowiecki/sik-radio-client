#ifndef SIK_RADIO_CLIENT_H
#define SIK_RADIO_CLIENT_H

#include <string>
#include <cinttypes>
#include "common.h"

enum struct IpVersion {
  AUTO,
  IPV4,
  IPV6
};

struct ClientConfig {
  ParsedUrl url;
  bool multiplexing = false;
  int timeout = 5000;
  IpVersion ip_version = IpVersion::AUTO;
  uint8_t verbosity = 2;
};

class SikClient {
public: 
  SikClient(const ClientConfig &config);
  ~SikClient();

  void run();

private: 
  ParsedUrl parse_url(const std::string& url);


  ClientConfig config;
};

#endif
