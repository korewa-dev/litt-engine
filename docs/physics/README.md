# Physics Documentation

GPU-accelerated rigid body physics with multi-tier hardware support.

## Files

| File | Content |
|------|---------|
| [physics-system.md](./physics-system.md) | PhysicsBody component, broadphase/narrowphase, integrators |

## Hardware Targeting

| Hardware | Acceleration Path |
|----------|-------------------|
| AMD RDNA  | GPU compute (WGSL) |
| ARM Mali  | NEON intrinsics |
| RISC-V    | RVV vectorized |
| Intel Arc | DirectML inference |
| Moore Threads | MUSA compute |

## Degradation

When GPU acceleration is unavailable, physics falls back to CPU simulation with fixed-step integration.
