#ifndef KAYLES_COMMON_H
#define KAYLES_COMMON_H

#include <cstdint>
#include <string>

#define BYTE_SIZE 8

struct ParsedUrl {
  bool is_ssl;
  std::string host;
  uint16_t port;
  std::string path;
};

uint16_t read_port(char const *str);
uint16_t read_port(const std::string& str);
void install_signal_handler(int signal, void (*handler)(int), int flags);
struct sockaddr_in get_server_address(std::string const& host, uint16_t port);
struct sockaddr_in get_server_address(char const *host, uint16_t port);

#endif
