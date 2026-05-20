#include <algorithm>
#include <cstring>
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

SikClient::SikClient(const ClientConfig &cfg)
    : config(cfg), buffer(BUFFER_SIZE), finish(0) {
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
}

void SikClient::run() {
  connect_to_server(config.url.host, config.url.port, config.ip_version);
  send_request();
  struct pollfd poll_descriptors[CONNECTIONS];

  // The main socket (for connection with the server).
  poll_descriptors[RADIO_POLL_IDX].fd = socket_fd;
  poll_descriptors[RADIO_POLL_IDX].events = POLLIN;

  // The second socket (for listening to the client "quit").
  poll_descriptors[USER_INPUT_POLL_IDX].fd = STDIN_FILENO;
  poll_descriptors[USER_INPUT_POLL_IDX].events = POLLIN;

  do {
    // Cleaning `poll_descriptors` array.
    for (int i = 0; i < CONNECTIONS; i++) {
      poll_descriptors[i].revents = 0;
    }

    // TODO: Czy config.timeout działa tutaj poprawnie, tj cyz nie może się
    // zresetowac przy czymś innym przypadkiem
    int poll_status = poll(poll_descriptors, CONNECTIONS, config.timeout);

    if (poll_status == -1) {
      // TODO: POLLERR?
      if (errno == EINTR) {
        printf("interrupted system call\n");
      } else {
        syserr("poll");
      }
    } else if (poll_status > 0) {
      if (!finish && (poll_descriptors[RADIO_POLL_IDX].revents & POLLIN)) {
        handle_radio_data();
        if(current_state == RadioState::REDIRECTING) {
          reconnect();
          send_request();
          poll_descriptors[RADIO_POLL_IDX].fd = socket_fd;
        }
      }
      if (!finish && (poll_descriptors[USER_INPUT_POLL_IDX].revents & POLLIN)) {
        handle_user_input();
      }
    } else if (poll_status == 0) {
      reconnect();
      send_request();

      poll_descriptors[RADIO_POLL_IDX].fd = socket_fd;
    }
  } while (!finish);

  if (poll_descriptors[RADIO_POLL_IDX].fd >= 0) {
    close(poll_descriptors[RADIO_POLL_IDX].fd);
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
  send_request();
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

    ssize_t received = read_data(temp_buff, bytes_to_read);

    if (received < 0) {
      syserr("read error from radio");
    } else if (received == 0) {
      // Server closed the connection.
      finish = 1;
      return;
    }

    buffer.write(temp_buff, received);

    process_buffer();
  } while (config.url.is_ssl && SSL_pending(ssl) > 0);
}

void SikClient::handle_user_input() {
  char temp_buff[TEMP_BUFFER_SIZE];

  // Leave one character for end of string ('\0').
  ssize_t received = read(STDIN_FILENO, temp_buff, sizeof(temp_buff) - 1);

  if (received > 0) {
    temp_buff[received] = '\0';
    std::string quit = "quit";
    if (std::strncmp(temp_buff, quit.c_str(), quit.size()) == 0) {
      finish = 1;
    }
  }
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
    }
  }
}

bool SikClient::process_headers() {
  std::string line;

  while (buffer.read_line(line)) {

    // Empty line - end of headers.
    if (line == "\r" || line == "") {
      if (!redirect_url.empty()) {
        current_state = RadioState::REDIRECTING;
      } else {
        current_state = RadioState::PLAYING_MUSIC;
        bytes_until_meta = icy_metaint;
      }
      return true;
    }

    std::string lower_line = line;
    std::transform(lower_line.begin(), lower_line.end(), lower_line.begin(),
                   ::tolower);

    // HTTP 302
    std::string search_loc = "location: ";
    if (lower_line.find(search_loc) == 0) {
      redirect_url = line.substr(search_loc.length());
      if (!redirect_url.empty() && redirect_url.back() == '\r') {
        redirect_url.pop_back();
      }
    }

    std::string search_key = "icy-metaint:";
    size_t pos = lower_line.find(search_key);

    if (pos != std::string::npos) {
      std::string value_str = line.substr(pos + search_key.length());

      icy_metaint = std::stoull(value_str);
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

  if (write(STDOUT_FILENO, temp, read_bytes) < 0) {
    syserr("write to stdout failure");
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

  std::string meta_str;
  meta_str.resize(current_metadata_length);
  buffer.read(&meta_str[0], current_metadata_length);

  fprintf(stderr, "%s\n", meta_str.c_str());

  current_state = RadioState::PLAYING_MUSIC;
  bytes_until_meta = icy_metaint;
  return true;
}

void SikClient::connect_to_server(const std::string &host, uint16_t port,
                                  IpVersion ip_version) {
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
  int errcode = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
  if (errcode != 0) {
    fatal("getaddrinfo: %s", gai_strerror(errcode));
  }

  socket_fd = -1;
  for (rp = result; rp != nullptr; rp = rp->ai_next) {
    socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (socket_fd == -1) {
      continue;
    }

    if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
    }

    close(socket_fd);
  }

  freeaddrinfo(result);

  if (rp == nullptr) {
    fatal("Could not connect to the server.");
  }

  if (config.url.is_ssl) {
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, socket_fd);
    SSL_set_tlsext_host_name(ssl, config.url.host.c_str());

    if (SSL_connect(ssl) <= 0) {
      ERR_print_errors_fp(stderr);
      fatal("SSL handshake failed");
    }
  }
}

void SikClient::send_request() {
  std::string request = "";
  request += "GET " + config.url.path + " HTTP/1.1\r\n";
  request += "Host: " + config.url.host + "\r\n";
  request += "Connection: Keep-Alive\r\n";

  if (config.multiplexing) {
    request += "Icy-MetaData: 1\r\n";
  }
  request += "\r\n";

  ssize_t written_length = write_data(request);
  if (written_length < 0) {
    syserr("written");
  } else if (written_length != request.size()) {
    fatal("incomplete writen");
  }
}
