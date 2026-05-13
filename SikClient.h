#ifndef SIK_RADIO_CLIENT_H
#define SIK_RADIO_CLIENT_H

#include "common.h"
#include <cinttypes>
#include <string>

enum struct IpVersion { AUTO, IPV4, IPV6 };

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
  // Creates socket, binds and connects to given server address, with respect
  // to its config. Returns `socket_fd`;
  void connect_to_server(const std::string &host, uint16_t port,
                         IpVersion ip_version);

  ClientConfig config;
  int socket_fd;
};

#endif
