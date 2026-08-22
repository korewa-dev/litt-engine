Title: NPU System Rules

> **Core Rule:** The NPU is the ideal path for maximum neural intelligence and performance. However, the engine gracefully degrades to GPU/CPU when NPU is unavailable. This isn''t a bug -- it''s by design.

---

## 1. NPU-Exclusive AI (When Available)

When an NPU is present, neural inference runs there exclusively for optimal performance:

### Supported Inference Domains (All Run on NPU)

| # | Domain | Description | Priority |
|---|--------|-------------|----------|
| 1 | NPC Thinking & Behavior | Dynamic behavior, strategy adaptation, emotional states, learning, prediction, coordination, memory, RL, emergent behavior |  Core |
| 2 | Player Behavior Prediction | Movement patterns, combat habits, puzzle-solving style, exploration, aggression vs stealth, timing |  Core |
| 3 | Procedural Generation | Levels, dungeons, quests, puzzles, loot, enemy waves, terrain, weather |  High |
| 4 | Dialogue & Personality | LLM-like models for NPC dialogue, personality shifts, emotional reactions, memory, social behavior |  High |
| 5 | Animation & Movement | Neural animation blending, locomotion, IK, ragdolls, gesture prediction, facial animation |  High |
| 6 | Combat Decision Making | Attack selection, dodge prediction, defense timing, weapon choice, target prioritization, group tactics, flanking |  High |
| 7 | World Simulation | Crowd behavior, ecosystems, weather logic, faction AI, traffic AI, wildlife AI, economy |  Medium |
| 8 | Audio AI | Voice synthesis, modulation, classification, vocal reactions, environmental audio prediction |  Medium |
| 9 | Graphics-Adjacent AI | Neural upscaling (mobile), denoising, texture generation, material prediction, lighting estimation |  Medium |
| 10 | Compression & Streaming | Asset decompression, neural texture/mesh compression, streaming prediction, bandwidth reduction |  Medium |
| 11 | Anti-Cheat & Security | Cheat detection, anomaly detection, input pattern analysis, bot detection, exploit prediction |  Core |

---

## 2. Graceful Degradation (When NPU Unavailable)

When no NPU is present, the engine adapts:

### NPU Detection Logic

```rust
pub enum NpuDetection {
    // Primary accelerators
    AMD_XDNA,      // Ryzen AI (XDNA 1/2)
    Intel_AI,      // Intel AI Boost
    ARM_NPU,       // Qualcomm Hexagon, MediaTek APU, Huawei DaVinci
    Samsung_NPU,   // Exynos RDNA iGPU + NPU
    RISC_V_NPU,    // Sophgo, VectorTile
    NVIDIA_TENSOR, // Tensor Cores (also GPU)
    Intel_XMX,     // Intel Arc XMX
    // Secondary accelerators
    ARM_GPU_ML,    // Mali GPU ML
    AMD_RDNA_ML,   // RDNA compute ML
    MooreThreads_ML, // MUSA compute ML
    // Fallback
    None,
}
```

### Fallback Behavior

| NPU Present | AI Domain | Hardware Path | Intelligence Level |
|-------------|-----------|---------------|-------------------|
|  AMD XDNA | All 11 domains | XDNA 50-50 TOPS | Maximum |
|  Intel AI Boost | All 11 domains | Intel 48 TOPS | Maximum |
|  ARM NPU | All 11 domains | Hexagon 15-12 TOPS | Maximum |
|  NPU absent | 1-6 domains | GPU SIMD (AVX2/AVX-512/NEON) | Medium |
|  NPU absent | 7-11 domains | CPU scalar | Low |
|  NPU absent | All domains | No AI | Basic behavior |

### Adaptation Strategies

```rust
pub enum AdaptationStrategy {
    // When NPU present
    NPU_Exclusive,    // All AI domains on NPU
    NPU_Hybrid,      // NPU + GPU for intensive tasks
    // When NPU absent  
    GPU_Fallback,    // SIMD-accelerated NPC behavior
    CPU_Scripted,    // Predefined scripts + basic reasoning
    Hybrid_Mixed,   // Mix of scripted + computed responses
}
```

---

## 3. Engine Design Philosophy

### Built for AI Agents (Not Humans)

The entire project is optimized for AI consumption:

- **Shaders**: Plain GLSL files editable with natural language
- **Components**: Flat Rust structures, no complex APIs
- **Actions**: Logged in `template/agent/actions.log` for audit
- **World State**: Predictable ECS with clear interfaces
- **Documentation**: Self-contained, no hidden steps

### Hardware Diversity as Feature

| Hardware | NPU | GPU | CPU | Intelligence |
|----------|-----|-----|-----|-------------|
| **Modern**: Ryzen AI + RDNA 4 |  AMD XDNA 2 (50 TOPS) |  RDNA 4 ML |  AVX-512 | Maximum |
| **Intel**: Arc + XeSS |  Intel AI Boost (48 TOPS) |  Arc iGPU |  AVX2 | Maximum |
| **Mobile**: Snapdragon + Adreno |  Hexagon (15 TOPS) |  Adreno |  NEON | High |
| **ARM**: Kirin + Mali |  DaVinci (8 TOPS) |  Mali GPU ML |  NEON | Medium |
| **Embedded**: RISC-V |  NPU optional |  Vortex GPU |  RVV | Low |
| **Generic**: No AI silicon |  NPU absent |  Integrated (RDNA/Vulkan) |  AVX2/NEON | Medium |

### Ultra-Lightweight Constraint

Target: **< 1 MB binary**
- Math crate: 0 dependencies
- Platform crate: ~20 KB
- ECS core: Minimal allocations
- No heavy abstractions
- Zero-cost optimizations

### ECS as Data Layout (Not Pattern)

The ECS is a memory layout strategy for hardware optimization:

- **Flat Data**: Components stored contiguously
- **Sequential Access**: Systems iterate efficiently
- **No VTables**: Zero dynamic dispatch in hot paths
- **Cache-Friendly**: Structure-of-Arrays (SoA)
- **GPU Ready**: Direct compute shader access

### Graceful Degradation Examples

#### When NPU Available: NPC Behavior
```rust
pub fn update_neural_ai(&mut self, dt: f32) {
    // Full neural inference on NPU
    let input = self.extract_observation();
    let output = npu_context.inference(input);
    self.apply_neural_output(output);
}
```

#### When NPU Unavailable: NPC Behavior
```rust
pub fn update_neural_ai(&mut self, dt: f32) {
    // Hybrid approach: NPU available?
    if let Some(npu) = self.check_npu_availability() {
        let input = self.extract_observation();
        let output = npu.inference(input);
        self.apply_neural_output(output);
    } else {
        // CPU/GPU fallback with reduced capabilities
        let input = self.extract_observation();
        let output = self.cpu_fallback_reasoning(input);
        self.apply_scripted_output(output);
    }
}
```

---

## 4. Roadmaps (Updated for Graceful Degradation)

### NPU-Ready vs NPU-Absent Plans

| Phase | With NPU | Without NPU |
|-------|-----------|-------------|
| 1 | Full neural AI | Scripted behavior |
| 2 | NPU-optimized graphics | Basic GPU rendering |
| 3 | ML-based physics | Simple Newtonian |
| 4 | NPU-driven NPCs | Rule-based NPCs |
| 5 | Real-time adaptation | Static behavior |

### Hardware-Specific Goals

#### RDNA / AMD
- Wave32 compute for NPU inference
- Async compute for AI + graphics overlap
- Temperature-based throttling

#### ARM / Mobile
- NEON for CPU fallback
- Hexagon NPU detection
- Power-efficient scaling

#### RISC-V
- RVV vectorization for CPU fallback
- Software RT for graphics
- Minimal memory footprint

#### Intel
- AI Boost integration
- XeSS AI upscaling
- DirectML backend

---

> **Core Principle:** The NPU represents the optimal path for AI intelligence, but Litt Engine''s strength lies in its graceful adaptation to any hardware configuration. The same code runs at maximum capability on ideal hardware, and at reduced but functional capability on minimal hardware. This makes Litt Engine truly universal -- it will always run, always provide value, and scale with the available hardware.

