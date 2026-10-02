# Resilient TCP/TLS Audio Client

A high-performance internet radio client written in modern C++20. It connects to streaming servers, buffers continuous byte streams over raw TCP/TLS sockets, and pipes pure audio data in real time for external playback.

Built without high-level HTTP or networking libraries (except OpenSSL for TLS), this project demonstrates robust handling of TCP stream fragmentation, asynchronous I/O polling, and protocol demultiplexing.

> NOTE: written as assignment on Networks course on MIMUW.

## Key Technical Features

- **TLS/SSL Encryption:** Integrates `libssl` and `libcrypto` to seamlessly establish secure TLS connections for HTTPS audio streams alongside standard HTTP streams.
- **Asynchronous I/O Polling:** Utilizes `poll()` to concurrently monitor network sockets and standard input without multi-threading overhead, ensuring responsive timeout management and graceful shutdowns.
- **Custom Circular Buffer:** Employs a custom-built, highly efficient circular buffer to decouple network ingest from `stdout`. This handles TCP fragmentation and prevents audio underruns.
- **Stream Demultiplexing:** Safely parses raw HTTP byte streams, extracting chunked ICY metadata (routed to `stderr`) from binary audio data (routed to `stdout`) without corrupting the audio pipe.
- **Protocol Agnostic:** Uses `getaddrinfo` for automatic, dual-stack IPv4 and IPv6 resolution.
- **HTTP State Management:** Manages HTTP redirects (3xx status codes) and session cookies (`Set-Cookie`) automatically during the handshake phase.

## Build and Run

### Dependencies
- C++20 compatible compiler (e.g., `g++`)
- OpenSSL (`libssl-dev`, `libcrypto`)
- `make`

### Compilation

```bash
make
```

### Usage
Connect to a stream, request metadata (`-m`), set a 3000ms timeout (`-t`), and pipe the standard output to an external audio player like `mpv` or `play`:

```bash
./sikradio -u http://stream.example.com:8000 -m -t 3000 | mpv --really-quiet -
```

### Options
- `-u <url>` : The URL of the radio stream (supports `http://` and `https://`).
- `-m` : Request multiplexed ICY text metadata from the server.
- `-t <timeout>` : Connection timeout in milliseconds (default: 5000).
- `-4` / `-6` : Force IPv4 or IPv6 resolution.
- `-v <level>` : Verbosity level (0-4) for diagnostic output on `stderr`.
- `-q` : Quiet mode (equivalent to `-v 0`).
