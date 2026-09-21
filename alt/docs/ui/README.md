# UI Documentation

HUD overlays, menus, and debug tools.

## Files

| File | Content |
|---|---|
| [ui-overlay-system.md](./ui-overlay-system.md) | UI hierarchy, layout system, debug overlay |

## Rendering status

CPU/software UI rendering is the only direction compatible with the current promoted graphics contract. Vulkan and DX12 UI paths are design targets only because the generic hardware backends are unavailable.

| Path | Status |
|---|---|
| CPU / software | Partial / experimental UI surface |
| Vulkan GPU font atlas | Design target, not release-supported |
| DX12 GPU font atlas | Design target, not release-supported |

See [../graphics-api/graphics-api-status.md](../graphics-api/graphics-api-status.md) for backend status.
