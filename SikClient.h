#ifndef SIK_RADIO_CLIENT_H
#define SIK_RADIO_CLIENT_H

#include <cinttypes>
#include <poll.h>
#include <string>
#include <signal.h>

#include "CircularBuffer.h"
#include "common.h"

enum struct IpVersion { AUTO, IPV4, IPV6 };

enum struct RadioState {
  READING_HEADERS,
  PLAYING_MUSIC,
  READING_METADATA_LENGTH,
  READING_METADATA
};

struct ClientConfig {
  ParsedUrl url;
  bool multiplexing = false;
  int timeout = 5000; // in milliseconds 
  IpVersion ip_version = IpVersion::AUTO;
  uint8_t verbosity = 2;
};

class SikClient {
public:
  // Can result in std::bad_alloc when allocating `CircularBuffer` buffer.
  SikClient(const ClientConfig &config);
  ~SikClient();

  void run();

private:
  // Creates socket, binds and connects to given server address, with respect
  // to its config. Returns `socket_fd`;
  void connect_to_server(const std::string &host, uint16_t port,
                         IpVersion ip_version);

  // Sends GET request to the server. Fails with `fatal` or `syserr` when write
  // error occurs.
  void send_request();

  // Following two functions are responsible for handling information from the
  // server (radio) and user (user input) respectively.
  void handle_radio_data();
  void handle_user_input();

  // Following five functions are responsible for handling data in the buffer
  // depending on the state in which the buffer is. `process_buffer` acts as 
  // a disposer for following functions. Program stays in this `process_buffer`
  // loop until the whole buffer is processed or the buffer state has changed.
  // `true` represents fulfilment of the above condition.
  void process_buffer();
  bool process_headers();
  bool extract_music();
  bool read_metadata_length();
  bool extract_metadata();

  ClientConfig config;
  int socket_fd;
  CircularBuffer buffer;

  // TODO: czy to powinein być atomic na pewno?
  volatile sig_atomic_t finish; // Flag for graceful shutdown.
 
  RadioState current_state = RadioState::READING_HEADERS; // Buffer state.
  size_t current_metadata_length = 0;
  size_t bytes_until_meta = 0; // Till the next metadata.
  size_t icy_metaint = 0;
};

#endif
