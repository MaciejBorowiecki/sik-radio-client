#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <ctime>
#include <netdb.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "CircularBuffer.h"
#include "SikClient.h"
#include "common.h"
#include "err.h"

#define BUFFER_SIZE 65536
#define TEMP_BUFFER_SIZE 4096
#define CONNECTIONS 2
#define RADIO_POLL_IDX 0
#define USER_INPUT_POLL_IDX 1
#define HTTP_STATUS_SUCCESS 200
#define HTTP_STATUS_REDIRECT_MIN 300
#define HTTP_STATUS_REDIRECT_MAX 400

SikClient::SikClient(const ClientConfig &cfg)
    : config(cfg), socket_fd(-1), buffer(BUFFER_SIZE), finish(0) {
  ctx = SSL_CTX_new(TLS_client_method());
  SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
}

SikClient::~SikClient() {
  if (ssl) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
  }
  if (ctx) {
    SSL_CTX_free(ctx);
  }
  if (socket_fd >= 0) {
    close(socket_fd);
  }
}

void SikClient::run() {
  const ParsedUrl original_url = config.url; // Redirect to original url.

  connect_to_server(config.url.host, config.url.port, config.ip_version);
  send_request();
  struct pollfd poll_descriptors[CONNECTIONS];

  // The main socket (for connection with the server).
  poll_descriptors[RADIO_POLL_IDX].fd = socket_fd;
  poll_descriptors[RADIO_POLL_IDX].events = POLLIN;

  // The second socket (for listening to the client "quit").
  poll_descriptors[USER_INPUT_POLL_IDX].fd = STDIN_FILENO;
  poll_descriptors[USER_INPUT_POLL_IDX].events = POLLIN;

  auto last_radio_data_time = std::chrono::steady_clock::now();
  do {
    // Cleaning `poll_descriptors` array.
    for (int i = 0; i < CONNECTIONS; i++) {
      poll_descriptors[i].revents = 0;
    }

    auto now = std::chrono::steady_clock::now();
    int elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - last_radio_data_time)
                         .count();

    int current_timeout = std::max(0, config.timeout - elapsed_ms);

    int poll_status = poll(poll_descriptors, CONNECTIONS, current_timeout);

    if (poll_status == -1) {
      if (errno == EINTR) {
        error(config.verbosity, "Poll interrupted");
      } else {
        syserr(config.verbosity, "poll");
      }
    } else if (poll_status > 0) {
      if (!finish && (poll_descriptors[RADIO_POLL_IDX].revents &
                      (POLLIN | POLLERR | POLLHUP))) {
        if (poll_descriptors[RADIO_POLL_IDX].revents & POLLERR) {
          error(config.verbosity, "Socket error, reconnecting...");
          config.url = original_url;
          cookies.clear();
          reconnect();
          send_request();
          poll_descriptors[RADIO_POLL_IDX].fd = socket_fd;
          last_radio_data_time = std::chrono::steady_clock::now();
        } else {
          last_radio_data_time = std::chrono::steady_clock::now();

          handle_radio_data();
          if (current_state == RadioState::REDIRECTING) {
            log_info(config.verbosity, 4, "Got redirected to the adress: %s\n",
                     redirect_url.c_str());
            config.url = parse_url(redirect_url, config.verbosity);
            redirect_url = "";
            reconnect();
            send_request();
            poll_descriptors[RADIO_POLL_IDX].fd = socket_fd;

            last_radio_data_time = std::chrono::steady_clock::now();
          }
        }
      }
      if (!finish && (poll_descriptors[USER_INPUT_POLL_IDX].revents & POLLIN)) {
        if (!handle_user_input()) {
          poll_descriptors[USER_INPUT_POLL_IDX].fd = -1;
        }
      }
    } else if (poll_status == 0) {
      log_info(config.verbosity, 1, "data receiving timeout\n");
      config.url = original_url;
      cookies.clear();
      reconnect();
      send_request();
      poll_descriptors[RADIO_POLL_IDX].fd = socket_fd;

      last_radio_data_time = std::chrono::steady_clock::now();
    }
  } while (!finish);

  // Write what is left in buffer (graceful shutdown).
  process_buffer();

  if (poll_descriptors[RADIO_POLL_IDX].fd >= 0) {
    close(poll_descriptors[RADIO_POLL_IDX].fd);
    socket_fd = -1;
  }
}

void SikClient::reconnect() {
  if (ssl) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
    ssl = nullptr;
  }
  if (socket_fd >= 0) {
    close(socket_fd);
    socket_fd = -1;
  }

  clear_buffer_and_state();

  connect_to_server(config.url.host, config.url.port, config.ip_version);
}

void SikClient::clear_buffer_and_state() {
  buffer.clear();
  current_state = RadioState::READING_HEADERS;
  bytes_until_meta = 0;
  current_metadata_length = 0;
  icy_metaint = 0;
}

ssize_t SikClient::read_data(void *buf, size_t len) {
  if (config.url.is_ssl) {
    return SSL_read(ssl, buf, len);
  }
  return read(socket_fd, buf, len);
}

ssize_t SikClient::write_data(const std::string &str) {
  if (config.url.is_ssl) {
    return SSL_write(ssl, str.c_str(), str.size());
  }
  return writen(socket_fd, str);
}

void SikClient::handle_radio_data() {
  // This needs to be in do-while loop for SSL_read to correctly read whole
  // package.
  do {
    uint8_t temp_buff[TEMP_BUFFER_SIZE];

    size_t space_left = buffer.size_writeable();

    size_t bytes_to_read = std::min(sizeof(temp_buff), space_left);

    if (bytes_to_read == 0) {
      // Buffer is full. `process_buffer` was already called after the last
      // write and couldn't extractd data (e.g. waiting for more datat to
      // complete metadata block for ssl connections). Return and wait for
      // the next POLLIN event. `extract_music` will drain buffer.
      return;
    }

    ssize_t received = read_data(temp_buff, bytes_to_read);

    if (received < 0) {
      syserr(config.verbosity, "read error from radio");
    } else if (received == 0) {
      log_info(config.verbosity, 1, "Server closed the connection.\n");
      // Server closed the connection.
      finish = 1;
      return;
    } else {
      log_info(config.verbosity, 4, "Read %zd bytes from the socket.\n",
               received);
    }

    buffer.write(temp_buff, received);

    process_buffer();
  } while (config.url.is_ssl && SSL_pending(ssl) > 0);
}

bool SikClient::handle_user_input() {
  char temp_buff[TEMP_BUFFER_SIZE];
  ssize_t received = read(STDIN_FILENO, temp_buff, sizeof(temp_buff) - 1);
  if (received == 0) {
    return false; // Stop monitoring on EOF
  }

  std::string stdin_buffer;
  if (received > 0) {
    stdin_buffer.append(temp_buff, received);
    if (stdin_buffer.find("quit\n") != std::string::npos) {
      finish = 1;
    }
    if (stdin_buffer.size() > 4096) {
      stdin_buffer.erase(0, stdin_buffer.size() - 5);
    }
  }
  return true;
}

void SikClient::process_buffer() {
  bool state_changed_or_data_consumed = true;

  while (state_changed_or_data_consumed && buffer.size_readable() > 0) {
    state_changed_or_data_consumed = false;

    switch (current_state) {
      case RadioState::READING_HEADERS:
        state_changed_or_data_consumed = process_headers();
        break;
      case RadioState::PLAYING_MUSIC:
        state_changed_or_data_consumed = extract_music();
        break;
      case RadioState::READING_METADATA_LENGTH:
        state_changed_or_data_consumed = read_metadata_length();
        break;
      case RadioState::READING_METADATA:
        state_changed_or_data_consumed = extract_metadata();
        break;
      default:
        break;
    }
  }
}

bool SikClient::process_headers() {
  std::string line;
  bool status_parsed = false;
  bool is_redirect = false;

  while (buffer.read_line(line)) {
    log_info(config.verbosity, 1, "%s\n", line.c_str());

    // Empty line - end of headers.
    if (line == "\r" || line == "") {
      if (is_redirect) {
        if (redirect_url.empty()) {
          fatal(config.verbosity, "Redirect without Location header");
        }
        current_state = RadioState::REDIRECTING;
      } else {
        current_state = RadioState::PLAYING_MUSIC;
        bytes_until_meta = icy_metaint;
      }
      return true;
    }
    if (!status_parsed) {
      // Parse http code from the first line.
      status_parsed = true;
      size_t sp = line.find(' ');
      if (sp == std::string::npos) {
        fatal(config.verbosity, "Invalid HTTP response");
      }
      int status = 0;
      try {
        status = std::stoi(line.substr(sp + 1));
      } catch (const std::exception &) {
        fatal(config.verbosity, "Invalid HTTP status code");
      }
      if (status == HTTP_STATUS_SUCCESS) {
        is_redirect = false;
      } else if (status >= HTTP_STATUS_REDIRECT_MIN &&
                 status < HTTP_STATUS_REDIRECT_MAX) {
        is_redirect = true;
      } else {
        fatal(config.verbosity, "Server returned error status: %d", status);
      }
    }

    std::string lower_line = line;
    std::transform(lower_line.begin(), lower_line.end(), lower_line.begin(),
                   ::tolower);

    // Redirecting.
    std::string search_loc = "location: ";
    if (lower_line.find(search_loc) == 0) {
      redirect_url = line.substr(search_loc.length());
      size_t start = redirect_url.find_first_not_of(" \t");
      size_t end = redirect_url.find_last_not_of(" \t\r");
      redirect_url = (start != std::string::npos)
                         ? redirect_url.substr(start, end - start + 1)
                         : "";
    }

    // Cookies.
    std::string search_cookie = "set-cookie: ";
    if (lower_line.find(search_cookie) == 0) {
      std::string cookie_val = line.substr(search_cookie.length());
      if (!cookie_val.empty() && cookie_val.back() == '\r') {
        cookie_val.pop_back();
      }

      // Get only name=value.
      size_t semicolon_pos = cookie_val.find(';');
      std::string name_value = (semicolon_pos != std::string::npos)
                                   ? cookie_val.substr(0, semicolon_pos)
                                   : cookie_val;

      size_t eq_pos = name_value.find('=');
      if (eq_pos != std::string::npos) {
        cookies[name_value.substr(0, eq_pos)] = name_value.substr(eq_pos + 1);
      }
    }

    // Metadata.
    std::string search_key = "icy-metaint:";
    size_t pos = lower_line.find(search_key);

    if (pos != std::string::npos) {
      std::string value_str = line.substr(pos + search_key.length());

      try {
        icy_metaint = static_cast<size_t>(std::stoull(value_str));
      } catch (const std::exception &) {
        error(config.verbosity,
              "Invalid icy-metaint value, disabling multiplexing");
        icy_metaint = 0;
      }
    }
  }

  return false;
}

bool SikClient::extract_music() {
  size_t available = buffer.size_readable();

  if (available == 0) {
    return false;
  }

  size_t to_read = available;
  // Ensure to not push metadata to music player.
  if (config.multiplexing && icy_metaint > 0) {
    to_read = std::min(available, bytes_until_meta);
  }

  uint8_t temp[TEMP_BUFFER_SIZE];
  to_read = std::min(to_read, sizeof(temp));

  size_t read_bytes = buffer.read(temp, to_read);

  if (writen(STDOUT_FILENO, temp, read_bytes) < 0) {
    syserr(config.verbosity, "write to stdout failure");
  }

  if (config.multiplexing && icy_metaint > 0) {
    bytes_until_meta -= read_bytes;

    if (bytes_until_meta == 0) {
      current_state = RadioState::READING_METADATA_LENGTH;
    }
  }

  return true;
}

bool SikClient::read_metadata_length() {
  if (buffer.size_readable() < 1) {
    return false;
  }

  uint8_t length_byte;
  buffer.read(&length_byte, 1);

  // ICY/Shoutcast metadata:
  // first byte = metadata length / 16
  // actual length is: length_byte * 16
  // Ref:
  // https://stackoverflow.com/questions/14540380/title-of-current-icecast-streamed-song
  current_metadata_length = length_byte * 16;

  if (current_metadata_length > 0) {
    current_state = RadioState::READING_METADATA;
  } else {
    current_state = RadioState::PLAYING_MUSIC;
    bytes_until_meta = icy_metaint;
  }

  return true;
}

bool SikClient::extract_metadata() {
  if (buffer.size_readable() < current_metadata_length) {
    return false;
  }

  std::string meta_str(current_metadata_length, '\0');
  buffer.read(&meta_str[0], current_metadata_length);

  // Delete metadata null padding.
  size_t actual_len = meta_str.find('\0');
  if (actual_len == std::string::npos)
    actual_len = current_metadata_length;

  if (actual_len > 0) {
    fwrite(meta_str.data(), 1, actual_len, stderr);
  }
  fprintf(stderr, "\n");

  current_state = RadioState::PLAYING_MUSIC;
  bytes_until_meta = icy_metaint;
  return true;
}

void SikClient::connect_to_server(const std::string &host, uint16_t port,
                                  IpVersion ip_version) {
  if (config.verbosity >= 1) {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    log_info(config.verbosity, 1, "%04d.%02d.%02d %02d.%02d.%02d\n",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
             tm.tm_min, tm.tm_sec);
  }
  log_info(config.verbosity, 1, "resolving name %s\n", host.c_str());

  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  if (ip_version == IpVersion::IPV4) {
    hints.ai_family = AF_INET;
  } else if (ip_version == IpVersion::IPV6) {
    hints.ai_family = AF_INET6;
  } else {
    hints.ai_family = AF_UNSPEC;
  }

  struct addrinfo *result, *rp;
  std::string port_str = std::to_string(port);
  std::string resolved_host = host;
  if (!host.empty() && host.front() == '[' && host.back() == ']') {
    resolved_host = host.substr(1, host.size() - 2);
  }
  int errcode =
      getaddrinfo(resolved_host.c_str(), port_str.c_str(), &hints, &result);
  if (errcode != 0) {
    fatal(config.verbosity, "getaddrinfo: %s", gai_strerror(errcode));
  }

  socket_fd = -1;
  for (rp = result; rp != nullptr; rp = rp->ai_next) {
    socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (socket_fd == -1) {
      continue;
    }

    // log info
    char ip_str[INET6_ADDRSTRLEN];
    void *addr;
    if (rp->ai_family == AF_INET) {
      addr = &((struct sockaddr_in *)rp->ai_addr)->sin_addr;
    } else {
      addr = &((struct sockaddr_in6 *)rp->ai_addr)->sin6_addr;
    }
    inet_ntop(rp->ai_family, addr, ip_str, sizeof(ip_str));

    if (rp->ai_family == AF_INET6) {
      log_info(config.verbosity, 1, "connecting to server [%s]:%d\n", ip_str,
               port);
    } else {
      log_info(config.verbosity, 1, "connecting to server %s:%d\n", ip_str,
               port);
    }
    if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
    }

    error(config.verbosity,
          "Failed to connect to one of the resolved addresses");
    close(socket_fd);
    socket_fd = -1;
  }

  freeaddrinfo(result);

  if (rp == nullptr) {
    fatal(config.verbosity, "Could not connect to the server.");
  }

  if (config.url.is_ssl) {
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, socket_fd);
    SSL_set_tlsext_host_name(ssl, config.url.host.c_str());

    if (SSL_connect(ssl) <= 0) {
      ERR_print_errors_fp(stderr);
      fatal(config.verbosity, "SSL handshake failed");
    }
  }
}

void SikClient::send_request() {
  std::string request = "";
  request += "GET " + config.url.path + " HTTP/1.1\r\n";
  request += "Host: " + config.url.host + "\r\n";
  request += "Connection: Keep-Alive\r\n";

  if (!cookies.empty()) {
    request += "Cookie: ";
    bool first = true;
    for (const auto &[name, value] : cookies) {
      if (!first)
        request += "; ";
      request += name + "=" + value;
      first = false;
    }
    request += "\r\n";
  }

  if (config.multiplexing) {
    request += "Icy-MetaData: 1\r\n";
  }
  request += "\r\n";

  ssize_t written_length = write_data(request);
  if (written_length < 0) {
    syserr(config.verbosity, "written");
  } else if (static_cast<size_t>(written_length) != request.size()) {
    fatal(config.verbosity, "incomplete writen");
  }
  log_info(config.verbosity, 1, "%s", request.c_str());
}
