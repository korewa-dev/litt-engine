# ECS Reference

> Complete reference for all ECS components and systems in the Litt Engine.

**Status:**  Reference doc -- covers all known components and systems across implemented and planned subsystems.

---

## Components

### Core Components (Implemented)

| Component | Module | Fields | Written By | Read By |
|-----------|--------|-------|-----------|---------|
| `Transform` | template | `position: Vec3`, `rotation: Quat`, `scale: Vec3` | PhysicsSystem, MovementSystem | RenderSystem, CameraSystem |
| `Camera` | template | `position: Vec3`, `rotation: Vec2`, `fov: f32`, `near_plane: f32`, `far_plane: f32`, `aspect: f32`, `exposure: f32` | CameraSystem | RenderSystem |
| `Player` | template | `position: Vec3`, `rotation: Vec2`, `velocity: Vec3`, `speed: f32`, `look_speed: f32`, `is_ground: bool` | InputSystem, PhysicsSystem | CameraSystem, RenderSystem |
| `Mesh` | template | `vertices: Vec<Vertex>`, `indices: Vec<u32>`, `bounding_box: Option<Bbox>` | AssetPipeline (planned) | RenderSystem |
| `Material` | template | `albedo: Vec3`, `roughness: f32`, `metallic: f32`, `ior: f32`, `emissive: Vec3`, `light_intensity: f32` | AssetPipeline (planned) | RenderSystem, PathTracer |
| `Light` | template | `position: Vec3`, `direction: Vec3`, `color: Vec3`, `intensity: f32`, `radius: f32` | LightSystem, AssetPipeline | RenderSystem, PathTracer |

### AI Components (Planned)

| Component | Fields | Written By | Read By |
|-----------|--------|-----------|---------|
| `NeuralBrain` | `model: u32`, `state: [f32; 256]`, `confidence: f32`, `input_shape: [u32; 3]`, `output_shape: [u32; 3]`, `task_queue: [u32; 8]`, `task_head: u32`, `task_tail: u32`, `memory_pool: u32` | NeuralAISystem | NeuralAISystem, DialogueSystem |
| `BehaviorState` | `mode: BehaviorMode`, `confidence: f32`, `target_entity: Option<Entity>`, `transition_timer: f32`, `emotional_vector: [f32; 8]` | NeuralAISystem | NeuralAISystem, CombatDecisionSystem |
| `MovementIntent` | `desired_velocity: Vec3`, `desired_direction: Vec3`, `target_position: Vec3` | NeuralAISystem | PhysicsSystem, MovementSystem |
| `CombatIntent` | `target_entity: Option<Entity>`, `action: CombatAction`, `cooldown: f32`, `priority: u32` | NeuralAISystem, CombatDecisionSystem | PhysicsSystem |
| `EmotionalState` | `anger: f32`, `fear: f32`, `curiosity: f32`, `aggression: f32`, `social: f32`, `excitement: f32`, `calm: f32`, `stress: f32` | NeuralAISystem | BehaviorState, DialogueSystem |
| `NpcMemory` | `recent_events: Vec<Event>`, `player_habits: PlayerProfile`, `faction_reputation: f32` | NeuralAISystem | BehaviorState |

### Physics Components (Planned)

| Component | Fields | Written By | Read By |
|-----------|--------|-----------|---------|
| `PhysicsBody` | `shape: ColliderShape`, `mass: f32`, `linear_velocity: Vec3`, `angular_velocity: Vec3`, `linear_damping: f32`, `angular_damping: f32`, `friction: f32`, `restitution: f32`, `layer: u32`, `is_trigger: bool`, `gravity_scale: f32` | PhysicsSystem (writer) | PhysicsSystem, RenderSystem |
| `CollisionEvent` | `entity_a: Entity`, `entity_b: Entity`, `normal: Vec3`, `penetration: f32`, `impact_velocity: f32` | PhysicsSystem | GameLogicSystem |

### Input Components (Planned)

| Component | Fields | Written By | Read By |
|-----------|--------|-----------|---------|
| `InputState` | `pressed: Vec<Action>`, `released: Vec<Action>`, `held: Vec<Action>`, `analog: Vec<(AnalogInput, f32)>`, `mouse_position: Vec2`, `mouse_delta: Vec2`, `gyro_delta: Vec3` | InputSystem | PlayerSystem, CameraSystem, UIOverlaySystem |

### UI Components (Planned)

| Component | Fields | Written By | Read By |
|-----------|--------|-----------|---------|
| `UIOverlay` | `layer: UILayer`, `z_index: f32`, `visible: bool`, `anchors: Vec<Anchor>` | UIOverlaySystem | RenderSystem |
| `UIText` | `content: String`, `font_size: u32`, `color: Color`, `alignment: TextAlign` | UIOverlaySystem | RenderSystem |
| `UIButton` | `label: String`, `on_click: Option<Action>`, `hover_color: Color`, `pressed_color: Color` | UIOverlaySystem | InputSystem |
| `UIPanel` | `children: Vec<Entity>`, `layout: LayoutType`, `padding: Vec2`, `background: Option<Color>` | UIOverlaySystem | RenderSystem |

### Network Components (Planned)

| Component | Fields | Written By | Read By |
|-----------|--------|-----------|---------|
| `NetworkEntity` | `net_id: u32`, `server_timestamp: u64`, `replication_mask: ReplicationMask`, `predicted_state: Option<PredictedState>` | NetworkingSystem | NetworkingSystem, RenderSystem |

---

## Systems

### Implemented Systems

| System | Reads | Writes | Execution Order | Status |
|--------|-------|--------|----------------|--------|
| `MovementSystem` | `Transform`, `Velocity` | `Transform` | 1 |  |
| `CameraSystem` | `Player`, `Transform`, `Camera` | `Camera` | 2 |  |
| `LightSystem` | `Light` | `Light` | 3 |  |

### Planned Systems

| System | Reads | Writes | Execution Order | Status |
|--------|-------|--------|----------------|--------|
| `InputSystem` | Raw HID (platform) | `InputState` | 0 |  |
| `NeuralAISystem` | `NeuralBrain`, `BehaviorState`, `Transform`, `InputState` | `MovementIntent`, `CombatIntent`, `BehaviorState`, `EmotionalState` | 1 |  |
| `PlayerSystem` | `Player`, `InputState`, `Transform` | `Player`, `Transform` | 2 |  |
| `PhysicsSystem` | `PhysicsBody`, `Transform`, `MovementIntent` | `Transform`, `CollisionEvent` | 3 |  |
| `RenderSystem` | `Renderable`, `Transform`, `Mesh`, `Material`, `Light`, `Camera` | Command buffers | 4 |  |
| `UIOverlaySystem` | `UIOverlay`, `UIText`, `UIButton`, `InputState` | `UIOverlay`, debug stats | 5 |  |
| `NetworkingSystem` | `NetworkEntity`, `InputState` | `NetworkEntity`, `Transform` (replicated) | 6 |  |

### Full Execution Order

```
0. InputSystem        -- Read HID, write InputState
1. NeuralAISystem     -- NPU inference, write MovementIntent/CombatIntent
2. PlayerSystem       -- Process input, update Player/Transform
3. PhysicsSystem      -- Simulate physics, write Transform, emit CollisionEvent
4. CameraSystem       -- Follow player, update Camera
5. RenderSystem       -- ECS -> GPU, record command buffers
6. UIOverlaySystem    -- Render HUD, menus, debug overlay
7. NetworkingSystem   -- Replicate entities, handle snapshots
```

---

## Query Patterns

### Single-component query
```cpp
for (auto entity : world.query_entities<Transform>()) {
    auto& transform = world.get_component<Transform>(entity);
    // ...
}
```

### Two-component query
```cpp
for (auto entity : world.query_entities_with<Player, Transform>()) {
    auto& player = world.get_component<Player>(entity);
    auto& transform = world.get_component<Transform>(entity);
    // ...
}
```

### Query with exclusion
```cpp
// Entities with Transform but NOT PhysicsBody (static objects)
for (auto entity : world.query_entities<Transform>()) {
    if (!world.has_component<PhysicsBody>(entity)) {
        // Static object -- skip physics
    }
}
```

### Mutable iteration
```cpp
for (auto entity : world.query_entities_with<Transform, Velocity>()) {
    auto& transform = world.get_component_mut<Transform>(entity);
    auto& velocity = world.get_component<Velocity>(entity);
    transform.position += velocity.linear * dt;
}
```

---

## Roadmap

### Short-term (1-3 months)
- [ ] Add `InputState` component and `InputSystem`
- [ ] Add `Renderable` component (mesh + material handle)
- [ ] Wire `RenderSystem` to renderer

### Mid-term (3-12 months)
- [ ] Add all AI components (`NeuralBrain`, `BehaviorState`, etc.)
- [ ] Add `PhysicsBody` component and `PhysicsSystem`
- [ ] Add UI components and `UIOverlaySystem`
- [ ] Add `NetworkEntity` component and `NetworkingSystem`

### Long-term (1-3 years)
- [ ] Component serialization for save/load
- [ ] Component versioning for hot-reload
- [ ] Multi-world support (separate ECS worlds per scene)

### Experimental
-  Dynamic component addition/removal at runtime
-  Component-level change detection (dirty tracking)
-  Cross-world component queries

### Hardware-Specific
- **RDNA / AMD:** No specific ECS requirements
- **Moore Threads:** Minimal ECS overhead for MUSA compatibility
- **ARM / Mobile:** Compact component layout for cache efficiency
- **RISC-V:** Minimal memory footprint, no dispatch


