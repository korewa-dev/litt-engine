# Input Documentation

Input aggregation, mapping, and platform abstraction.

## Files

| File | Content |
|------|---------|
| [input-system.md](./input-system.md) | HID event collection, mapping table, action vs state |

## Hardware Targeting

| Platform | Input Backend |
|----------|---------------|
| Windows  | Raw input APIs |
| Linux    | X11 / Wayland |
| Android | Android Input System |
| Steam Deck | HID + Sensor fusion |

## Degradation

When advanced input features (gyro, haptic) are unavailable, the system degrades to basic keyboard/gamepad input.

