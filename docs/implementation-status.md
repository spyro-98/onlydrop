# Implementation status

This repository starts from the supplied OnlyDrop v0.1 technical specification. The following foundations are implemented and compile on POSIX with C17:

- CMake project with strict warnings and optional AddressSanitizer/UndefinedBehaviorSanitizer flags;
- CLI defaults and validation for expiry, count, source selection, protocol selection, and output modes;
- complete pre-listener loading from a file, stdin, or `--text` into process-allocated anonymous memory;
- SHA-256 through OpenSSL EVP, CSPRNG URL tokens, filename sanitisation, and explicit zeroisation at cleanup;
- lifecycle state and transfer-count primitives;
- a libevent/evhttp server module that reports a copyable URL with the effective listener port;
- terminal QR rendering through libqrencode, with the exact printed URL as its payload;
- IPv4 LAN address selection that avoids loopback, VPN, Docker, and VM interfaces when a private LAN address is available.
- paused client connections remain active in `graceful` expiry mode; a detected client cancellation or network-write failure closes the drop without consuming a completed-download slot; `hard` expiry closes the active HTTP connection at its deadline.
- interactive transfer progress showing bytes sent, total size, percentage, throughput, and a best-effort paused-client indication after three seconds without outbound progress.

## Deliberately unfinished work

This is not an advertised v0.1 release yet. The remaining work is material:

- implement the ephemeral in-memory TLS certificate and HTTPS listener;
- complete signal handling, connection cancellation, precise transfer-success accounting, and listener shutdown semantics;
- add the acceptance-level end-to-end checks from the specification.

When libevent is unavailable, CMake builds the input/security foundation but the executable refuses to serve. That is intentional: it preserves the required `libevent/evhttp` architecture and avoids an unreviewed fallback networking stack.
