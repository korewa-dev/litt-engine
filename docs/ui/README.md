# UI Documentation

HUD overlays, menus, and debug tools.

## Files

| File | Content |
|------|---------|
| [ui-overlay-system.md](./ui-overlay-system.md) | UI hierarchy, layout system, debug overlay |

## Hardware Targeting

| Hardware | Rendering |
|----------|-----------|
| Vulkan    | GPU font atlas |
| DX12     | GPU font atlas |
| CPU      | Software bitmap fallback |

## Degradation

When GPU rendering is unavailable, UI falls back to software rendering with basic scaling.

