# AI Documentation

NPU acceleration, inference rules, and the NeuralAISystem.

## Files

| File | Content |
|------|---------|
| [npu-support.md](./npu-support.md) | NPU hardware table, detection, modes, precision |
| [npu-rules.md](./npu-rules.md) | NPU-exclusive inference rules, component types, telemetry |
| [neural-ai-system.md](./neural-ai-system.md) | NeuralAISystem pipeline, inference flow, NPC adaptation |

## Architecture

```
NeuralAISystem
    |
    v
NPU Context (async batch dispatch)
    |
    +---> AMD XDNA (Ryzen AI)
    +---> Intel AI Boost
    +---> Qualcomm Hexagon
    +---> Huawei Kirin
    +---> Samsung Exynos
    +---> RISC-V NPU
    |
    v
Output Components
    - MovementIntent
    - CombatIntent
    - BehaviorState
    - EmotionalState
```

## Core Rule

> All neural inference must run on the NPU only. No GPU inference. No CPU inference. No fallback paths.

See [npu-rules.md](./npu-rules.md) for the full rule set.

