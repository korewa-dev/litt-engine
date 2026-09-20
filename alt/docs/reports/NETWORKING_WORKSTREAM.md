# Networking Workstream Report

## Scope

Source directive: implement a minimal real local/loopback connect/listen/send/receive/disconnect transport before replication or matchmaking, with explicit failure behavior and end-to-end tests.

## Baseline audit

The repository previously had two misleading networking surfaces:

- `litt_networking.h` declared server/client/peer APIs without implementations.
- the generic `litt_engine_systems.h` NetworkManager correctly reported `Unavailable`.

Historical networking documentation described UDP, WebSocket, SteamNetworkingSockets, replication, prediction, and hardware-specific behavior that was not implemented or release-gated.

## Implemented milestone

`alt/src/native/littcore/litt_networking.h` now provides a dependency-free framed TCP transport under `litt::net`:

- `TcpServer::listen`, `accept`, `stop`
- `TcpClient::connect`, `disconnect`, `send`, `receive`
- movable `TcpPeer` accepted connections
- 16-byte versioned wire header using network byte order
- 64 KiB hard payload limit
- exact-length send/receive loops for TCP fragmentation
- explicit result codes for timeout, disconnect, refusal, malformed input, size violation, invalid arguments, and system errors
- POSIX sockets on Linux/macOS-compatible systems and Winsock on Windows
- no new third-party runtime dependency

Connection establishment is synchronous. Configurable timeouts apply to framed send/receive operations.

## Verification gate

`alt/src/native/littcore/networking_tests.cpp` covers:

1. loopback connect/listen/accept/send/receive
2. clean remote disconnect
3. refused connection
4. receive timeout
5. outbound payload size limit
6. reconnect lifecycle
7. multiple simultaneous clients
8. fragmented header and payload delivery
9. malformed wire magic
10. disconnect during a partial payload

Run locally with:

```sh
make -C alt/src/native network-test
```

The Stabilization workflow additionally compiles/runs the networking contract under ASan/UBSan on Linux and under MSVC/Winsock on Windows.

## Explicitly unsupported

This milestone does not claim UDP, WebSocket, SteamNetworkingSockets, TLS, matchmaking, NAT traversal, ECS replication, snapshots, prediction/reconciliation, lag compensation, or anti-cheat networking. The old design document remains future reference only.

The generic high-level `NetworkManager` in `litt_engine_systems.h` remains unavailable rather than pretending these higher-level systems exist.

## Gate status

The workstream's first green gate is implemented: one documented transport has a real end-to-end contract, explicit limits, predictable failure behavior, sanitizer coverage, and cross-platform build/run coverage.
