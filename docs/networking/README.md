# Networking Documentation

Networking for multiplayer and synchronization.

## Files

| File | Content |
|------|---------|
| [networking-system.md](./networking-system.md) | UDP/WebSocket, snapshot interpolation, ECS replication |

## Hardware Targeting

| Platform | Network Backend |
|----------|-----------------|
| Windows  | Winsock + SteamNetworkingSockets |
| Linux    | Raw socket APIs |
| Android | Custom TCP/IP stack |

## Degradation

When advanced networking features (compression, prediction) are unavailable, basic UDP/raw socket communication is used.

