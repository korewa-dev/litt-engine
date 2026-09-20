# Networking

The release-supported networking surface is deliberately small: a dependency-free, framed TCP transport in `alt/src/native/littcore/litt_networking.h`.

## Supported contract

- IPv4/IPv6 address resolution through the operating system
- loopback or normal TCP listen/accept/connect
- multiple accepted peers
- framed message send/receive
- explicit disconnect
- send/receive timeouts
- 64 KiB maximum payload
- malformed header and oversized inbound frame rejection
- fragmented TCP reads/writes handled by exact-length loops
- Linux/POSIX sockets and Windows Winsock

The wire header is 16 bytes: magic, version, reserved byte, message type, payload length, and message id. Integer fields are network byte order. Wire version is currently 1.

`TcpClient::connect` is synchronous and uses the operating system's TCP connect behavior. The configurable timeout applies to framed send and receive operations, not connection establishment.

## Failure behavior

Operations return `litt::net::Result`. Unsupported or invalid behavior does not report success. Malformed frames and inbound payloads larger than 64 KiB close the peer. A clean remote close returns `Disconnected`.

## Verification

Run:

```sh
make -C alt/src/native network-test
```

The Stabilization workflow also runs the contract under ASan/UBSan on Linux and builds/runs it with Winsock on Windows. Tests cover refused connections, clean disconnect, partial TCP delivery, malformed frames, payload limits, receive timeout, reconnect lifecycle, and multiple clients.

## Not supported yet

UDP, WebSocket, SteamNetworkingSockets, TLS, matchmaking, NAT traversal, replication, snapshot interpolation, prediction/reconciliation, lag compensation, and ECS synchronization are not part of this transport milestone. See `networking-system.md` only as a future design reference.
