# Litt Engine â€” Subsystem Documentation Prompt

You are documenting the **Litt Engine**, a Rust ECS-based game engine targeting AMD RDNA, Intel Arc, Moore Threads, ARM mobile, and RISC-V platforms. Your job is to fill documentation gaps â€” not rewrite what already exists.

---

## Step 1 â€” Audit Existing Docs

Read every file under `docs/` and the root `README.md`. Here is what you will find:

| File | What It Covers |
|------|---------------|
| `README.md` (root) | High-level overview, NPU table, FSR/XESS/RT features, quick start, build reqs |
| `ARCHITECTURE.md` | Crate dependency graph, render pipeline, VMA memory management, data flow, BLAS/TLAS pipeline |
| `ECS_ARCHITECTURE.md` | ECS crate structure, World API table, basic systems (Movement, Camera, Light) |
| `NPU_SUPPORT.md` | NPU hardware table (10+ NPUs), detection logic, modes, precision, env vars |
| `NPU_RULES.md` | NPU component types, inference rules, backend selection, telemetry |
| `FSR_SUPPORT.md` | FSR 3/4 versions, GPU support matrix, quality presets |
| `AMD_OPTIMIZATION.md` | RDNA shader/compiler optimizations, RGP integration |
| `DX12_SUPPORT.md` | DX12 backend architecture, building, env vars |
| `MOORE_THREADS.md` | MUSA GPU support, Vulkan extensions, driver notes |
| `INTEL_XESS3.md` | Intel Arc XeSS 3 integration, performance tips |
| `ROADMAP.md` | 15 phases of development with detailed checklists |
| `BINARY_SIZE.md` | Size targets and optimization flags |

**Rule:** Never duplicate content from these files. Cross-reference with `[see FILE.md]` instead.

---

## Step 2 â€” Create `docs/README.md` (Central Hub)

This is a **navigation index**, not a duplicate of the root `README.md`.

Structure:
```markdown
# Litt Engine â€” Documentation Hub

One-paragraph purpose: this directory contains subsystem-level documentation. For the high-level engine overview, see the root [README.md](../README.md). For the development timeline, see [ROADMAP.md](ROADMAP.md).

## Subsystems

| Subsystem | Doc | Status | Summary |
|-----------|-----|--------|---------|
| ECS Core | [ECS Architecture](ECS_ARCHITECTURE.md) | âœ… Complete | Core types, World API, query patterns |
| ECS Reference | [ECS Reference](ECSReference.md) | ðŸ”² New | Full component + system API table |
| Neural AI | [NeuralAISystem](NeuralAISystem.md) | ðŸ”² New | NPU inference pipeline, NPC behavior, model formats |
| Physics | [PhysicsSystem](PhysicsSystem.md) | ðŸ”² New | GPU compute physics, RDNA/ARM/RISC-V backends |
| Render | [RenderSystem](RenderSystem.md) | ðŸ”² New | ECSâ†’GPU draw pipeline, frame graph, shader compilation |
| Input | [InputSystem](InputSystem.md) | ðŸ”² New | Aggregation, mapping, action/state resolution |
| UI Overlay | [UIOverlaySystem](UIOverlaySystem.md) | ðŸ”² New | HUD, menus, debug overlays, text rendering |
| Networking | [NetworkingSystem](NetworkingSystem.md) | ðŸ”² New | UDP/WS backend, snapshot interpolation, replication |

## Deep-Dive Docs

| Topic | File |
|-------|------|
| Full architecture | [ARCHITECTURE.md](ARCHITECTURE.md) |
| NPU hardware table | [NPU_SUPPORT.md](NPU_SUPPORT.md) |
| NPU inference rules | [NPU_RULES.md](NPU_RULES.md) |
| FSR integration | [FSR_SUPPORT.md](FSR_SUPPORT.md) |
| Intel XESS 3 | [INTEL_XESS3.md](INTEL_XESS3.md) |
| AMD optimization | [AMD_OPTIMIZATION.md](AMD_OPTIMIZATION.md) |
| DX12 backend | [DX12_SUPPORT.md](DX12_SUPPORT.md) |
| Moore Threads | [MOORE_THREADS.md](MOORE_THREADS.md) |
| Development roadmap | [ROADMAP.md](ROADMAP.md) |
| Binary size | [BINARY_SIZE.md](BINARY_SIZE.md) |

## Cross-Subsystem Dependencies

```
InputSystem  â”€â”€â†’  NeuralAISystem  â”€â”€â†’  PhysicsSystem
     â”‚               â”‚                    â”‚
     â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
                     â”‚
              Transform component
                     â”‚
                     â–¼
              RenderSystem  â”€â”€â†’  UIOverlaySystem
                     â”‚
                     â–¼
              NetworkingSystem  (replication)
```

## Quick Links

- [Root README (overview + quick start)](../README.md)
- [Development Roadmap](ROADMAP.md)
- [Crate source tree](../crates/)
```

---

## Step 3 â€” Create Missing Subsystem Docs

### `docs/NeuralAISystem.md`

The engine has `NeuralBrain` and `BehaviorState` components defined in `template/agent/` and referenced in `NPU_RULES.md`, but **no document explains the ECS integration pipeline**. Write one.

Must cover:
- The inference pipeline: input observation â†’ NPU dispatch â†’ output component update
- How `NeuralBrain` holds model reference (ONNX/GGUF path), inference state, and config
- How `BehaviorState` tracks current behavior (idle, patrol, combat, flee) and transitions
- How `MovementIntent` and `CombatIntent` are written by the AI system and consumed by PhysicsSystem / CombatSystem
- Backend selection via `BackendSelector::best_available()` from `NPU_RULES.md` (cross-reference, don't repeat)
- Model loading: which formats, which crates handle conversion
- NPC adaptation: how repeated player behavior updates model weights or behavior tree

Include:
- A Mermaid sequence diagram of one inference tick
- Pseudocode of the system update loop
- Roadmap (see Step 5)

**Do NOT** re-list NPU hardware specs â€” those are in `NPU_SUPPORT.md`.
**Do NOT** re-explain inference rules â€” those are in `NPU_RULES.md`.

---

### `docs/PhysicsSystem.md`

Roadmap Phase 5 marks this as PLANNED. No implementation exists yet, but the component (`PhysicsBody`) and system name (`PhysicsSystem`) are in the roadmap. Write a design doc.

Must cover:
- `PhysicsBody` component fields: collider shape enum (AABB/sphere/capsule), mass, velocity, angular damping, friction, restitution
- Broadphase: Spatial Hash or SAP (Sort and Prune) â€” GPU vs CPU paths
- Narrowphase: SAT for AABB, GJK-EPA for convex shapes
- Rigid body integrator: semi-implicit Euler, impulse-based resolution
- ECS integration: how PhysicsSystem reads `PhysicsBody`, writes `Transform`, and emits collision events
- GPU compute path: RDNA WGSL compute kernels for broadphase, ARM NEON for mobile fallback, RISC-V RVV for scalar
- Async compute: physics runs in parallel with rendering on separate command queues

Include:
- A Mermaid class diagram of `PhysicsBody` and related types
- Pseudocode of the physics tick
- Roadmap (see Step 5)

---

### `docs/RenderSystem.md`

The engine has `litt-renderer`, `litt-pathtracer`, and `litt-fidelityfx` crates, plus Vulkan and DX12 backends. No single doc explains **how the ECS drives rendering**. Write one.

Must cover:
- The ECSâ†’GPU pipeline: `Renderable` component â†’ command buffer recording â†’ draw dispatch
- Frame graph: how passes are ordered (clear â†’ depth â†’ opaque â†’ transparent â†’ UI â†’ post-process)
- Shader compilation: GLSL â†’ SPIR-V (Vulkan) and HLSL â†’ DXIL (DX12), handled at build time via `build.rs`
- How `Transform` + `Mesh` + `Material` components combine into a draw call
- Render pass architecture: what passes exist and in what order
- FidelityFX integration point: where denoising, upscaling, and frame generation fit in the frame graph
- DX12 vs Vulkan command recording differences

Include:
- A Mermaid flowchart of the frame graph
- Pseudocode of the render system update loop
- Roadmap (see Step 5)

**Do NOT** repeat FSR details â€” those are in `FSR_SUPPORT.md`.
**Do NOT** repeat RDNA optimizations â€” those are in `AMD_OPTIMIZATION.md`.
**Do NOT** repeat DX12 backend details â€” those are in `DX12_SUPPORT.md`.

---

### `docs/InputSystem.md`

The roadmap lists `InputSystem` as complete but no doc exists. The component `InputState` is referenced but not defined in code yet. Write a design doc.

Must cover:
- Input aggregation: keyboard, mouse, gamepad â€” how raw HID events are collected
- Input mapping: action â†’ input binding table (configurable via TOML/JSON)
- Action vs state distinction: actions are discrete (jump, shoot), states are continuous (move, sprint)
- `InputState` component: what fields it holds, how it's queried by other systems
- Platform abstraction: how `litt-platform` hides OS-specific input
- Steam Deck profile: gyro, trackpad, face buttons mapped to actions
- ECS integration: InputSystem writes `InputState`, PlayerSystem reads it

Include:
- A Mermaid sequence diagram of one input frame
- Example TOML input mapping config
- Roadmap (see Step 5)

---

### `docs/UIOverlaySystem.md`

Roadmap Phase 9 lists UI as PLANNED. No implementation exists. Write a design doc.

Must cover:
- HUD layer: health bar, ammo, minimap â€” rendered as overlay on top of scene
- Menu layer: pause menu, settings, main menu â€” full-screen overlay with navigation
- Debug overlay: FPS counter, entity count, draw calls, GPU timer, backend name
- ECS UI components: `UIOverlay`, `UIText`, `UIButton`, `UIPanel`
- Text rendering: font atlas approach, atlas rebuilding on font change
- Layout system: stack, grid, flex â€” how UI elements are positioned
- Interaction: how UI clicks route through InputSystem to game actions
- Debug overlay integration with `GraphicsBackend` profiling hooks

Include:
- A Mermaid component diagram of the UI layer hierarchy
- Pseudocode of the UI update loop
- Roadmap (see Step 5)

---

### `docs/NetworkingSystem.md`

Roadmap Phase 10 lists networking as PLANNED. Write a design doc.

Must cover:
- UDP client/server: packet structure, sequencing, acknowledgment
- Snapshot interpolation: how client reconciles server state with local prediction
- ECS replication: which components are replicated, authority model (server-owned vs client-owned)
- WebSocket backend: browser target support via `wasm-bindgen`
- `SteamNetworkingSockets` optional module: when it's available vs bare UDP
- `NetworkEntity` component: entity ID, replication mask, server timestamp
- Latency compensation: client-side prediction, server reconciliation, lag compensation for shooting

Include:
- A Mermaid sequence diagram of a snapshot round-trip
- Packet structure pseudocode
- Roadmap (see Step 5)

---

### `docs/ECSReference.md`

`ECS_ARCHITECTURE.md` covers the crate API at a high level. Write a **complete component and system reference**.

Must cover:

**Components** (table format, one per component):
| Component | Fields | Purpose | System Writes |
|-----------|--------|---------|---------------|
| `Transform` | position, rotation, scale | World-space pose | MovementSystem, PhysicsSystem |
| `Camera` | position, rotation, fov, near/far, aspect, exposure | View frustum + HDR | CameraSystem |
| `Player` | position, rotation, velocity, speed, look_speed, is_ground | Player controller state | InputSystem, PhysicsSystem |
| `Mesh` | vertices, indices, bounding_box | GPU geometry | AssetPipeline (import) |
| `Material` | albedo, roughness, metallic, ior, emissive, light_intensity | PBR shader params | AssetPipeline (import) |
| `Light` | position, direction, color, intensity, radius | Illumination source | LightSystem |
| `NeuralBrain` | model_path, input_shape, output_shape, state | NPU model reference | NeuralAISystem |
| `BehaviorState` | state, confidence, target_entity, action_queue | Current AI behavior | NeuralAISystem |
| `MovementIntent` | linear, angular, priority | Desired movement | NeuralAISystem, InputSystem |
| `CombatIntent` | target, action, cooldown, priority | Combat decision | NeuralAISystem |
| `PhysicsBody` | collider, mass, velocity, angular_damp, friction, restitution | Rigid body state | PhysicsSystem |
| `InputState` | actions, axes, gamepad | Aggregated input | InputSystem |
| `Renderable` | mesh, material, transform_ref | Draw call data | RenderSystem |
| `UIOverlay` | panel_type, visibility, z_order | UI element | UIOverlaySystem |
| `NetworkEntity` | entity_id, replication_mask, server_tick | Network state | NetworkingSystem |

**Systems** (table format):
| System | Reads | Writes | Execution Order |
|--------|-------|--------|----------------|
| `InputSystem` | HID events | InputState | 1st |
| `NeuralAISystem` | InputState, entity positions | BehaviorState, MovementIntent, CombatIntent | 2nd |
| `PhysicsSystem` | PhysicsBody, MovementIntent | Transform | 3rd |
| `CameraSystem` | Player, Transform | Camera | 4th |
| `RenderSystem` | Transform, Mesh, Material, Camera | CommandBuffer | 5th |
| `UIOverlaySystem` | InputState, UIOverlay | UI commands | 6th |
| `NetworkingSystem` | Transform, PhysicsBody | NetworkEntity, packets | 7th |

**Query patterns** (code examples for each):
- Single-component query
- Two-component query
- Query with exclusion
- Iterating with mutable access

---

## Step 4 â€” Roadmap Format (Strict)

Every subsystem file must end with this exact structure:

````markdown
## Roadmap

### Short-term (1â€“3 months)
- [ ] Goal 1
- [ ] Goal 2

### Mid-term (3â€“12 months)
- [ ] Goal 1
- [ ] Goal 2

### Long-term (1â€“3 years)
- [ ] Goal 1
- [ ] Goal 2

### Experimental
- ðŸ’¡ Idea 1
- ðŸ’¡ Idea 2

### Hardware-Specific
- **RDNA / AMD:** ...
- **Intel Arc:** ...
- **ARM / Mobile:** ...
- **RISC-V:** ...
````

---

## Step 5 â€” Quality Rules

1. **Tone:** Technical, concise, code-oriented. Match the style of `NPU_RULES.md` and `ARCHITECTURE.md`.
2. **No fluff:** Every paragraph must convey information. No introductory filler.
3. **Cross-reference, don't repeat:** If `NPU_SUPPORT.md` already lists hardware specs, write `[see NPU_SUPPORT.md]` instead of re-typing the table.
4. **Diagrams first:** Every subsystem file must include at least one Mermaid diagram (sequence, class, or flowchart).
5. **Code over prose:** Prefer Rust pseudocode or actual code snippets over paragraphs.
6. **One subsystem per file:** Do not mix concerns.
7. **Link to source:** When referencing a component or system, link to the source file path (e.g., `template/src/components/transform.rs`).

---

## Step 6 â€” Output Checklist

Before you finish, verify:
- [ ] `docs/README.md` exists as a navigation hub
- [ ] Every new subsystem file has: summary, ECS integration, cross-subsystem notes, diagram, pseudocode, roadmap
- [ ] No content duplicates `ARCHITECTURE.md`, `NPU_SUPPORT.md`, `NPU_RULES.md`, `FSR_SUPPORT.md`, `AMD_OPTIMIZATION.md`, `DX12_SUPPORT.md`, `MOORE_THREADS.md`, `INTEL_XESS3.md`, or `ROADMAP.md`
- [ ] All cross-references use `[see FILE.md]` format
- [ ] Roadmap uses the exact format from Step 4
- [ ] `ECSReference.md` covers every component and system listed in Step 3
- [ ] Final folder: `docs/README.md` + 6 new subsystem files + existing docs unchanged

