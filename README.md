# Chat++
A command line websockets chat application written in C++17.

## build
This project can be built using cmake:
```
cmake -S . -B build
cmake --build build
```
This was compiled on Windows using MSVC and MinGW and on Linux using GCC and Clang.

### supported compilers
This project is tested to build using the following compilers:
#### Windows
- MSVC
- MinGW
#### Linux
- Clang
- GCC

## dependencies
Header-only libraries are vendored under `extern/`:
- Asio 1.32.0
- WebSocket++ 0.8.2
- nlohmann/json 3.12.0
- CLI11 2.7.2
- rang 3.3

Asio is pinned to 1.32.0 because newer releases removed the `io_service` API
still used by WebSocket++ 0.8.2.

TLS support is enabled by default and additionally requires OpenSSL. It can be
disabled at configure time with `-DCHATPP_ENABLE_TLS=OFF`.

## usage
Run `chatpp`, enter a nickname and the destination URI (for example `ws://127.0.0.1:9002`).
Every instance provides a relay server and connects to the URI you enter.

Options:
- `-H,--host` listening address (default `127.0.0.1`)
- `-p,--port` listening port (default `9002`)
- `--conf` configuration file (default `conf.ini`)
- `--tls-cert` server certificate chain in PEM format
- `--tls-key` server private key in PEM format
- `--tls-ca` CA bundle used to verify the destination server
- `--insecure` disable certificate verification for outgoing TLS
- `--server-token` token required from clients of this server
- `--token` token sent to the destination server
- `--max-connections` maximum simultaneous clients (default `100`)
- `--max-connections-per-ip` maximum simultaneous clients per address (default `5`)
- `--allow-plaintext` allow unencrypted non-local connections

## security
Plaintext is only allowed on loopback by default. A server that binds to a
non-local address must be configured with `--tls-cert` and `--tls-key`, and a
client that connects to a non-local `ws://` URI must pass `--allow-plaintext`.
This prevents accidentally exposing chat traffic on a network.

### TLS
Start a TLS server:
```
chatpp --tls-cert cert.pem --tls-key key.pem
```
and connect with a `wss://` URI. Clients verify the certificate and hostname
using the system CA store, or a bundle passed with `--tls-ca`. Use `--insecure`
only for testing; it disables certificate verification.

For a local self-signed certificate:
```
openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 \
  -nodes -subj "/CN=localhost" -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
```

### authentication
Set `--server-token` on the relay and `--token` on clients. The token is sent in
the `Authorization: Bearer` header during the WebSocket upgrade and compared in
constant time. Both values can instead be provided through the `CHATPP_TOKEN`
and `CHATPP_SERVER_TOKEN` environment variables.

Tokens are sent in plaintext over `ws://`; use `wss://` outside loopback.

### rate limits
Each connection is limited to 20 messages per second (burst 40) and 256 KiB per
second (burst 512 KiB). Connections above `--max-connections` or
`--max-connections-per-ip` are rejected, and clients with more than 1 MiB of
buffered outgoing data are disconnected. Abuse disconnects use WebSocket close
code 1008.

TLS protects the transport only. The relay server can read every message; this
is not end-to-end encryption.

## Licence
This project is distributed under the MIT licence.
