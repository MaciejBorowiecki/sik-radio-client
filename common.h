#ifndef KAYLES_COMMON_H
#define KAYLES_COMMON_H

#include <cstdint>
#include <string>

#define DEFAULT_PORT_HTTP 80
#define DEFAULT_PORT_HTTPS 443
#define BYTE_SIZE 8

struct ParsedUrl {
  bool is_ssl;
  std::string host;
  uint16_t port;
  std::string path;
};

uint16_t read_port(char const *str, uint8_t verbosity);
uint16_t read_port(const std::string &str, uint8_t verbosity);
void install_signal_handler(int signal, void (*handler)(int), int flags, uint8_t verbosity);
ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t writen(int fd, const std::string &str);
ParsedUrl parse_url(const std::string &url, uint8_t verbosity);
unsigned long ulong_from_str(int min_val, int max_val, const char *num_type,
                             const char *str, uint8_t verbosity);

void log_info(uint8_t current_verbosity, uint8_t target_level, const char* fmt, ...);

#endif
