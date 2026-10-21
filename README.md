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

## usage
Run `chatpp`, enter a nickname and the destination URI (for example `ws://127.0.0.1:9002`).
Every instance provides a relay server and connects to the URI you enter.

Options:
- `-H,--host` listening address (default `127.0.0.1`)
- `-p,--port` listening port (default `9002`)
- `--conf` configuration file (default `conf.ini`)

The connection uses plain `ws://` without encryption or authentication. Only bind
to a non-local address on a trusted network.

## Licence
This project is distributed under the MIT licence.
