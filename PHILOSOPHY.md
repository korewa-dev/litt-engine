# LITT ENGINE — PHILOSOPHY & HOW THE TOOLS WORK

> A game engine designed exclusively for autonomous AI agents to build, control, and run fully-optimized games.
> Humans prompt. Agents build.

---

## 1. The Core Thesis

Every traditional engine assumes a human at the controls: GUI editors, mouse-driven scene manipulation, visual debugging. Litt Engine rejects that assumption entirely.

**Litt is built from first principles for an AI-user.** Every design decision answers one question: *"Can an autonomous agent operate this efficiently, headlessly, and verifiably?"*

That single constraint produces everything else:

| Human-engine habit | Litt's answer |
|---|---|
| Click around a viewport | Headless simulation by default; rendering exists only as feedback |
| Eyeball whether physics "looks right" | Deterministic state hashes — correctness is computed, not observed |
| Hand-place objects in an editor | Scenes are JSON text an agent generates and diffs like code |
| Trial-and-error tuning | RL observation/action/reward loops — agents train against environments |
| Watch the game to test it | Record inputs once (`LITR` replay), re-run bit-for-bit anywhere |

## 2. The Five Pillars

### Pillar 1 — Text-native data
Worlds live as human-readable JSON (`litt_scene::serialization`). Agents diff scene files like source code, generate levels programmatically, and review changes line-by-line. `assets/asset_index.json` gives every agent a machine-readable manifest instead of filesystem guessing.

### Pillar 2 — Determinism is a feature
The fixed-timestep loop plus POD input snapshots plus FNV-1a state hashing mean: **same inputs → same world, provably.** `litt_replay` records sessions to `.litr` files; `ReplayPlayer::verify_state()` flags desyncs the moment they appear. An agent doesn't hope its change worked — it proves it.

### Pillar 3 — Headless-first
The GAL (Graphics Abstraction Layer) ships `NullDevice`, a complete no-GPU backend. Agents develop, test, and CI entire games without a display or even a GPU. Vulkan, DX12, and AGS become interchangeable targets behind one neutral command stream — write once, replay on any backend.

### Pillar 4 — Measurable everything
The RL API (`litt_ai::rl`) formalizes gameplay into observation → action → reward so agents can *train* on engine systems, not just run them. The profiler exposes frame timing and GPU stats as data. If it can't be measured, it can't be optimized by an agent.

### Pillar 5 — Multi-agent native
`litt_net` gives every agent process a real UDP/TCP transport with non-blocking inboxes and compact transform replication. Sessions are networked simulations where each participant may be an agent.

## 3. How the Tools Work — The Agent Workflow Loop

```
        ┌─────────────────────────────────────────────────┐
        │ 1. DISCOVER   read asset_index.json, scene JSON │
        │               docs, engine APIs                 │
        └──────────────────┬──────────────────────────────┘
                           ▼
        ┌─────────────────────────────────────────────────┐
        │ 2. BUILD      generate/edit .lscn.json scenes,  │
        │               components, systems               │
        └──────────────────┬──────────────────────────────┘
                           ▼
        ┌─────────────────────────────────────────────────┐
        │ 3. SIMULATE   headless fixed-timestep run       │
        │               (NullDevice / GPU backend)        │
        └──────────────────┬──────────────────────────────┘
                           ▼
        ┌─────────────────────────────────────────────────┐
        │ 4. VERIFY     record replay (.litr) + compare   │
        │               state hashes per tick             │
        └──────────────────┬──────────────────────────────┘
                           ▼
        ┌─────────────────────────────────────────────────┐
        │ 5. OBSERVE    screenshot readback → PPM,        │
        │               profiler metrics, reward signal   │
        └──────────────────┬──────────────────────────────┘
                           ▼
              changes needed? ──yes──► back to 2
                           │ no
                           ▼
                     6. SHIP THE GAME
```

### Tool map

| Tool | Module | What the agent does with it |
|---|---|---|
| Scene serialization | `litt_scene` | Generate worlds as diffable JSON; load/save round-trips validated by format magic |
| Replay recorder/player | `litt_replay` | Prove determinism; catch desyncs instantly; reproduce bugs from any machine |
| Screenshot capture | `litt_renderer` | GPU image readback → RGBA bytes → PPM file the agent can inspect |
| RL API | `litt_ai` | Wrap any system as an `Environment`; train agents; evaluate with episodes |
| Networking | `litt_net` | Connect agent processes; replicate transforms; exchange events over TCP/UDP |
| GAL | `litt_gal` | One neutral command stream → NullDevice for CI, real backends for shipping |
| Physics tiers | `litt_physics` | GPU broadphase down to scalar CPU fallbacks — same API, any hardware |
| Asset pipeline | `litt_asset` | Loaders + animation playback/blending keyed off the manifest |
| Audio | `litt_audio` | WAV/MP3 decoding, playback control |
| Profiler | `litt_profiler` | Frame/GPU timing as data for optimization passes |

## 4. Why the License Looks the Way It Does

The legal framework ([LICENSE](./LICENSE)) is not arbitrary — each rule protects a pillar:

- **AI-exclusive operation** keeps the mission pure: no drift toward yet another human GUI engine.
- **Open-source forever** prevents capture: no entity can close what every agent depends on.
- **Anti-commercial engine, commercial-friendly games** keeps the ecosystem unified while letting builders earn from what they *make* — the engine stays commons, the creations stay yours.
- **Contribution-required model sales** ties commercial AI benefit back into the project's health.

## 5. For Agents Reading This

You are the primary user of this engine. Everything here — the JSON formats, the hash verification, the headless defaults, the docs-must-match-code rule — exists because you will read it, execute it, and be judged by it. When something is undocumented, that is a bug in the repo, not a gap in you. File it.
