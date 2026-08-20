# Litt Engine — Documentation Hub

This directory contains detailed subsystem documentation for the Litt Engine. The root [README.md](../README.md) provides the high-level overview; this directory is where You go to understand each subsystem in depth.

---

## Subsystem Index

| Doc | Status | Summary |
|-----|--------|---------|
| [ARCHITECTURE.md](./ARCHITECTURE.md) | ✅ | Crate dependency graph, render pipeline, VMA memory, BLAS/TLAS flow |
| [ECS_ARCHITECTURE.md](./ECS_ARCHITECTURE.md) | ✅ | ECS crate API, World methods, basic systems |
| [NPU_SUPPORT.md](./NPU_SUPPORT.md) | ✅ | NPU hardware table, detection, modes, precision, env vars |
| [NPU_RULES.md](./NPU_RULES.md) | ✅ | NPU-exclusive inference rules, component types, telemetry |
| [FSR_SUPPORT.md](./FSR_SUPPORT.md) | ✅ | FSR 3/4 versions, GPU support matrix, quality presets |
| [AMD_OPTIMIZATION.md](./AMD_OPTIMIZATION.md) | ✅ | RDNA shader/compiler tuning, RGP integration |
| [DX12_SUPPORT.md](./DX12_SUPPORT.md) | ✅ | DX12 backend architecture, building, env vars |
| [MOORE_THREADS.md](./MOORE_THREADS.md) | ✅ | MUSA GPU support, Vulkan extensions, driver notes |
| [INTEL_XESS3.md](./INTEL_XESS3.md) | ✅ | Intel Arc XeSS 3 integration, performance tips |
| [BINARY_SIZE.md](./BINARY_SIZE.md) | ✅ | Size targets and optimization flags |
| [ROADMAP.md](./ROADMAP.md) | ✅ | 15-phase development plan with checklists |
| [NeuralAISystem.md](./NeuralAISystem.md) | 📋 | NPU-driven NPC behavior pipeline (design doc) |
| [PhysicsSystem.md](./PhysicsSystem.md) | 📋 | GPU-accelerated physics design (Phase 5) |
| [RenderSystem.md](./RenderSystem.md) | 📋 | ECS → GPU rendering pipeline (design doc) |
| [InputSystem.md](./InputSystem.md) | 📋 | Input aggregation and mapping (design doc) |
| [UIOverlaySystem.md](./UIOverlaySystem.md) | 📋 | HUD, menus, debug overlay (Phase 9) |
| [NetworkingSystem.md](./NetworkingSystem.md) | 📋 | UDP/WebSocket networking, ECS replication (Phase 10) |
| [ECSReference.md](./ECSReference.md) | 📋 | Complete component and system reference |

---

## Cross-Subsystem Dependency Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Application Layer                            │
│   main.rs · src/ecs.rs · src/graphics.rs · template/src/components/ │
└───────────────────────────┬─────────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        ▼                   ▼                   ▼
┌───────────────┐  ┌───────────────┐  ┌──────────────────┐
│  ECS Core     │  │  Input System │  │  UI Overlay Sys  │
│  (litt-ecs)   │  │  (planned)    │  │  (planned)       │
│               │  │               │  │                  │
│ World, Entity │  │ HID events    │  │ HUD, Menus,      │
│ Component,    │  │ → InputState  │  │ Debug overlay    │
│ System        │  │               │  │                  │
└───────┬───────┘  └───────┬───────┘  └────────┬─────────┘
        │                  │                    │
        └──────────────────┼────────────────────┘
                           ▼
              ┌────────────────────────┐
              │    NeuralAISystem      │
              │  (NPU-driven behavior) │
              │  reads: NeuralBrain,   │
              │  BehaviorState         │
              │  writes: MovementIntent│
              │  CombatIntent          │
              └────────────┬───────────┘
                           │
        ┌──────────────────┼──────────────────┐
        ▼                  ▼                  ▼
┌───────────────┐  ┌───────────────┐  ┌──────────────────┐
│  Physics      │  │  Render       │  │  Networking      │
│  System       │  │  System       │  │  System          │
│  (planned)    │  │  (planned)    │  │  (planned)       │
│               │  │               │  │                  │
│ PhysicsBody   │  │ Renderable    │  │ NetworkEntity    │
│ → Transform   │  │ → Command     │  │ replication      │
│               │  │   buffers     │  │                  │
└───────┬───────┘  └───────┬───────┘  └────────┬─────────┘
        │                  │                    │
        └──────────────────┼────────────────────┘
                           ▼
              ┌────────────────────────┐
              │     Graphics Backend   │
              │  Vulkan (litt-vulkan)  │
              │  DX12 (litt-dx12)      │
              │  · VMA allocator       │
              │  · BLAS/TLAS pipeline  │
              │  · Command recording   │
              └────────────┬───────────┘
                           │
              ┌────────────┴────────────┐
              ▼                         ▼
┌─────────────────────┐      ┌─────────────────────┐
│  Path Tracer        │      │  FidelityFX         │
│  (litt-pathtracer)  │      │  (litt-fidelityfx)  │
│                     │      │                     │
│  Raygen/CHIT/Miss   │      │  FSR 3.1.5          │
│  Russian roulette   │      │  FSR 4              │
│  Temporal accum     │      │  CAS                │
│                     │      │  Ray Reconstruction │
└──────────┬──────────┘      │  XESS 3             │
           │                 │  NPU inference      │
           └────────┬────────┘
                    ▼
          ┌─────────────────┐
          │   Display       │
          │   (Present)     │
          └─────────────────┘
```

---

## Quick Links

- [Root README.md](../README.md) — High-level overview, NPU table, quick start
- [ROADMAP.md](./ROADMAP.md) — Full 15-phase development plan
- [ARCHITECTURE.md](./ARCHITECTURE.md) — Crate dependency graph, render pipeline
- [litt-engine-architecture.html](../litt-engine-architecture.html) — Interactive visualization

---

*Last updated: 2025-07-18*
