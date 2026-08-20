# NPU System Rules

> **Core Rule:** All neural inference must run on the NPU only. No GPU inference. No CPU inference. No fallback paths. No alternative accelerators.

---

## 1. NPU-Exclusive Game AI & Neural Systems

The NPU is the dedicated neural processor for all real-time AI inference inside the game. It is the "brain" of the game world.

### Supported Inference Domains

| # | Domain | Description | Priority |
|---|--------|-------------|----------|
| 1 | NPC Thinking & Behavior | Dynamic behavior, strategy adaptation, emotional states, learning, prediction, coordination, memory, RL, emergent behavior | 🔴 Core |
| 2 | Player Behavior Prediction | Movement patterns, combat habits, puzzle-solving style, exploration, aggression vs stealth, timing | 🔴 Core |
| 3 | Procedural Generation | Levels, dungeons, quests, puzzles, loot, enemy waves, terrain, weather | 🟡 High |
| 4 | Dialogue & Personality | LLM-like models for NPC dialogue, personality shifts, emotional reactions, memory, social behavior | 🟡 High |
| 5 | Animation & Movement | Neural animation blending, locomotion, IK, ragdolls, gesture prediction, facial animation | 🟡 High |
| 6 | Combat Decision Making | Attack selection, dodge prediction, defense timing, weapon choice, target prioritization, group tactics, flanking | 🟡 High |
| 7 | World Simulation | Crowd behavior, ecosystems, weather logic, faction AI, traffic AI, wildlife AI, economy | 🟢 Medium |
| 8 | Audio AI | Voice synthesis, modulation, classification, vocal reactions, environmental audio prediction | 🟢 Medium |
| 9 | Graphics-Adjacent AI | Neural upscaling (mobile), denoising, texture generation, material prediction, lighting estimation | 🟢 Medium |
| 10 | Compression & Streaming | Asset decompression, neural texture/mesh compression, streaming prediction, bandwidth reduction | 🟢 Medium |
| 11 | Anti-Cheat & Security | Cheat detection, anomaly detection, input pattern analysis, bot detection, exploit prediction | 🟢 Medium |
| 12 | Physics-Adjacent AI | Neural collision prediction, ragdoll stabilization, trajectory prediction, damage modeling, vehicle control | 🟢 Medium |
| 13 | Reinforcement Learning | Small RL loops, reward evaluation, policy updates, adaptive difficulty, NPC evolution | 🟡 High |
| 14 | Engine Optimization | Frame prediction, workload scheduling, thermal management, battery optimization, resource allocation, AI-based LOD | 🟢 Medium |

---

## 2. Core NPU Components

### NeuralBrain Component

`"`"rust
/// Primary NPU-driven component attached to AI entities.
#[derive(Clone, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct NeuralBrain {
    /// Model handle loaded on the NPU
    pub model: u32,
    /// Inference state (f32 embeddings, hidden states)
    pub state: [f32; 256],
    /// Last inference timestamp (ms)
    pub last_inference_ms: f32,
    /// Inference confidence (0.0-1.0)
    pub confidence: f32,
    /// Model input shape (channels, height, width)
    pub input_shape: [u32; 3],
    /// Model output shape
    pub output_shape: [u32; 3],
    /// Pending inference tasks
    pub task_queue: [u32; 8],
    /// Task queue head/tail
    pub task_head: u32,
    pub task_tail: u32,
    /// Memory pool handle for NPU allocations
    pub memory_pool: u32,
}
`"`"

### NpuContext - Global NPU State

`"`"rust
/// Global NPU context managing inference pipelines.
pub struct NpuContext {
    pub device: NpuDevice,
    pub pipelines: Vec<NpuPipeline>,
    pub memory_allocator: NpuMemoryAllocator,
    pub inference_queue: AsyncInferenceQueue,
    pub telemetry: NpuTelemetry,
}
`"`"

### NpuPipeline - Model Pipeline

`"`"rust
/// A compiled neural model ready for inference on the NPU.
pub struct NpuPipeline {
    pub id: u32,
    pub model_type: NpuModelType,
    pub input_tensors: Vec<NpuTensor>,
    pub output_tensors: Vec<NpuTensor>,
    pub batch_size: u32,
    pub precision: NpuPrecision,
    pub latency_budget_ms: f32,
}
`"`"

### NpuTensor - Inference Tensor

`"`"rust
/// A tensor transferred to/from NPU memory.
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct NpuTensor {
    pub data_ptr: u64,       // Host-side buffer pointer (pinned memory)
    pub shape: [u32; 4],
    pub elements: u32,
    pub dtype: NpuPrecision,
    pub layout: NpuLayout,
}
`"`"

---

## 3. NPU Component Types

| Component | Storage | Description |
|-----------|---------|-------------|
| `NeuralBrain` | Entity | AI model reference + inference state + task queue |
| `NpuModelRef` | Shared | Lightweight handle to a loaded `NpuPipeline` |
| `InferenceRequest` | Command buffer | Pending inference task with input/output tensors |
| `NpcMemory` | Entity | Long-term memory of player actions and world events |
| `EmotionalState` | Entity | Current emotional vector (anger, fear, curiosity, etc.) |
| `BehaviorPolicy` | Shared | RL policy weights for NPC decision-making |
| `PlayerPredictor` | Entity | Predicted player position/behavior next N frames |
| `ProceduralSeed` | Entity | NPU-generated procedural content seed |
| `NeuralAnimationBlend` | Entity | Neural blend weights for animation system |
| `DialogueState` | Entity | Current dialogue tree node + personality context |

---

## 4. NPU Systems

| System | Description | Runs On |
|--------|-------------|---------|
| `NeuralAISystem` | NPU-driven behavior inference for NPCs | NPU |
| `PlayerPredictionSystem` | Predicts player movement and habits | NPU |
| `ProceduralGenerationSystem` | Generates levels, quests, loot on NPU | NPU |
| `DialogueSystem` | Runs LLM-like models for NPC dialogue | NPU |
| `NeuralAnimationSystem` | Computes neural animation blending and IK | NPU |
| `CombatDecisionSystem` | Handles combat AI on NPU | NPU |
| `WorldSimulationSystem` | Simulates crowds, ecosystems, factions | NPU |
| `AudioAISystem` | Voice synthesis and audio prediction | NPU |
| `AntiCheatSystem` | Runs anomaly detection models | NPU |
| `PhysicsPredictionSystem` | Neural collision and trajectory prediction | NPU |
| `RlTrainingSystem` | Online RL policy updates for NPCs | NPU |
| `EngineOptimizerSystem` | AI-based LOD, scheduling, thermal management | NPU |

---

## 5. NPU Inference Rules

### Rule 1: Exclusive NPU Inference
All neural model inference **must** run on the NPU. GPU compute shaders and CPU fallbacks are **not permitted** for inference tasks.

### Rule 2: Zero Fallback
If the NPU is unavailable, the engine does **not** fall back to GPU or CPU for inference. The affected subsystem is disabled or runs in a minimal deterministic mode.

### Rule 3: Asynchronous Batching
Inference requests are batched asynchronously. The host (CPU) submits requests and polls for completion - no synchronous blocking.

### Rule 4: Pinned Memory Only
All tensor data transferred to/from the NPU uses pinned (page-locked) host memory for maximum bandwidth.

### Rule 5: Precision Budget
Each `NpuPipeline` declares its precision budget. The engine enforces precision consistency - mixed-precision within a single inference is not allowed unless explicitly declared.

### Rule 6: Latency Isolation
Each inference domain has a latency budget. If a pipeline exceeds its budget, it is flagged for throttling - not for fallback.

### Rule 7: Memory Pooling
NPU memory allocations use a pool allocator. Tensors are allocated from pools, not individually, to minimize fragmentation and transfer overhead.

### Rule 8: Model Versioning
Models are versioned. Hot-reloading a model replaces the pipeline but does **not** invalidate existing inference state - state is transitioned gracefully.

---

## 6. NPU Backend Selection

`"`"rust
pub enum NpuBackend {
    /// AMD Ryzen AI (XDNA architecture)
    AmdXdna,
    /// Intel AI Boost (Movidius VPU)
    IntelAiBoost,
    /// Qualcomm Hexagon (mobile)
    QualcommHexagon,
    /// Apple Neural Engine
    AppleNe,
    /// MediaTek APU
    MediaTekApus,
    /// Huawei Kirin NPU
    KirinNpu,
    /// Samsung Exynos NPU
    SamsungNpu,
    /// RISC-V AI Accelerator
    RiscvNpu,
    /// Disabled - no NPU available
    Disabled,
}
`"`"

---

## 7. NPU Configuration

`"`"rust
pub struct NpuConfig {
    /// Selected backend
    pub backend: NpuBackend,
    /// Inference mode
    pub mode: NpuMode,          // Disabled / Auto / Forced / Hybrid
    /// Precision mask (bit 0=FP16, bit 1=INT8, bit 2=INT4, bit 3=BF16)
    pub precision_mask: u32,
    /// Max allowed inference latency per frame (ms)
    pub max_latency_ms: f32,
    /// Batch size for async inference
    pub batch_size: u32,
    /// Enable telemetry
    pub telemetry: bool,
}
`"`"

---

## 8. NPU Telemetry

| Metric | Description |
|--------|-------------|
| `inference_count` | Total inferences executed this frame |
| `avg_latency_ms` | Average inference latency |
| `peak_latency_ms` | Peak inference latency |
| `throughput_tops` | Current throughput in TOPS |
| `memory_used_mb` | NPU memory currently allocated |
| `batch_utilization` | Batch fill ratio (0.0-1.0) |
| `error_count` | Inference errors this frame |
| `model_hits` | Per-model cache hit rate |

---

## 9. Integration with ECS

`"`"rust
// Example: Creating an NPU-driven NPC
let npc = world.create_entity();
world.add_component(npc, Transform::default());
world.add_component(npc, NeuralBrain {
    model: 0,
    state: [0.0; 256],
    last_inference_ms: 0.0,
    confidence: 0.0,
    input_shape: [1, 64, 64],
    output_shape: [1, 32],
    task_queue: [0; 8],
    task_head: 0,
    task_tail: 0,
    memory_pool: 0,
});
world.add_component(npc, BehaviorState::default());
world.add_component(npc, MovementIntent::default());
world.add_component(npc, CombatIntent::default());
world.add_component(npc, EmotionalState::default());
world.add_component(npc, NpcMemory::new());
`"`"

`"`"rust
// Example: Registering NPU systems
world.add_system(NeuralAISystem::new(&npu_context));
world.add_system(PlayerPredictionSystem::new(&npu_context));
world.add_system(CombatDecisionSystem::new(&npu_context));
world.add_system(NeuralAnimationSystem::new(&npu_context));
`"`"

---

## 10. Error Handling

| Error Code | Meaning |
|------------|---------|
| `NPU_ERR_NOT_AVAILABLE` | No NPU detected on this hardware |
| `NPU_ERR_MODEL_LOAD` | Failed to load/compile a model |
| `NPU_ERR_INFER` | Inference failed (tensor mismatch, OOM) |
| `NPU_ERR_LATENCY` | Inference exceeded latency budget |
| `NPU_ERR_MEMORY` | NPU memory pool exhausted |
| `NPU_ERR_QUEUE_FULL` | Inference queue is full, batch dropped |

When an NPU error occurs, the affected system logs the error and **skips** the frame's inference for that domain. No fallback is attempted.
