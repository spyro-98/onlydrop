# OnlyDrop

Temporary, RAM-only LAN sharing from the terminal.

OnlyDrop reads one file, a pipe, or `--text` into anonymous process memory, then exposes it through a tokenised HTTP URL that automatically expires. The original source is closed after loading, so it may be renamed, moved, or deleted while the drop is available.

> **Project status:** early v0.1 development. HTTP, tokenised URLs, terminal QR codes, expiry policies, and one-at-a-time transfers are implemented. HTTPS with an in-memory ephemeral certificate is not implemented yet.

## Features

- no application-level disk fallback: the payload is loaded completely into RAM before networking starts;
- random URL-safe token generated with OpenSSL;
- file, stdin/pipe, or direct UTF-8 text input;
- SHA-256 output, copyable URL, and optional terminal QR code;
- one transfer at a time, with an expiring download window;
- `graceful` expiry lets an already-open client connection continue, including a paused download;
- `hard` expiry interrupts the active connection at the deadline;
- a detected client cancellation or network write error closes the drop without counting a completed download.

## Requirements

- CMake 3.20 or newer;
- C17 compiler;
- OpenSSL 3;
- libevent with `evhttp`;
- libqrencode.

On macOS with Homebrew:

```sh
brew install cmake openssl libevent qrencode pkgconf
```

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

The executable is `build/onlydrop`.

## Usage

```sh
# Share one file for one hour (the defaults)
./build/onlydrop archive.zip

# Show a terminal QR code and expire after ten minutes
./build/onlydrop movie.mkv --qr --expires 10m

# Use a fixed port or interface when necessary
./build/onlydrop archive.zip --bind 192.168.1.42 --port 49172

# Share piped data or direct text
git diff | ./build/onlydrop --name changes.diff
./build/onlydrop --text 'Hello world' --name message.txt

# Absolute deadline: interrupt the active client at expiry
./build/onlydrop secret.bin --expires 10m --expiry-mode hard
```

When ready, OnlyDrop prints a URL such as:

```text
http://192.168.1.42:49172/d/<random-token>
```

The same full URL is encoded by `--qr`. The token is the access credential: do not share it more widely than intended.

## Safety and current limitations

OnlyDrop creates no deliberate temporary file or disk-backed payload cache. RAM-only does not prevent operating-system swap, filesystem cache, crash dumps, or network buffers from retaining data outside the process.

This early version serves HTTP only. HTTPS, user-provided certificates, Range/resume support, multiple payloads, and concurrent transfers are intentionally out of scope until the core lifecycle is fully hardened.

## License

OnlyDrop is available under the [MIT License](LICENSE). It allows use, modification, distribution, sublicensing, and commercial use provided that the copyright and license notice are retained; it is supplied without warranty.
