# Litt Engine Ultimate Bug Fix Audit Backlog

Audit date: 2026-09-20

Umbrella task: #41

## Current verified baseline

The latest readiness branch revision passed the complete Stabilization workflow, including:

- core stabilization
- ASan/UBSan stabilization
- native C runtime contract
- generated-game contract
- C bridge contract
- Windows native build
- standalone compilation of every public littcore header

This means the supported test matrix is green. It does **not** mean every advertised subsystem is production-ready.

## Completed during readiness repair

- repaired standalone public-header dependencies across the littcore surface
- repaired stale WaveOut class/override and WAV format-state defects
- corrected invalid WorldKit regression expectations
- fixed EventDispatcher unsubscribe behavior and synchronization, with regressions
- removed an event unsubscribe use-after-free found by ASan
- replaced C bridge fake entity/transform success paths with real state
- removed the unused fixed 1920x1080x4 framebuffer allocation from each C engine handle
- made unavailable C bridge GPU/framebuffer data fail/report honestly
- added C bridge contract tests and Windows C bridge compilation to CI

## Open release tasks

### High

- #43 Define and gate the supported Litt runtime/API path
- #44 Complete or isolate audio loading/backend behavior
- #45 Consolidate renderer/GPU backend claims with actual implementations
- #46 Replace or isolate remaining FFI deployment stubs
- #48 WorldGen quality and resource-budget audit
- #50 Documentation and packaging truth pass

### Medium

- #47 Consolidate legacy litt_engine2 JSON runtime
- #49 Resource baseline and low-resource release budget

## Audit conclusions

### Supported-path status

The generated-game/native C path is the strongest executable contract in the repository. It has native tests, generated-game validation, and cross-platform CI evidence.

The C bridge is now gated and no longer silently pretends entity/transform operations succeeded.

### Experimental/incomplete surfaces

The generic C++ renderer exposes multiple named GPU backends while their initialization/resource/draw implementations remain unavailable or stubbed. These must be isolated/documented rather than advertised as ready until implemented.

AudioManager still exposes a successful-looking clip load path while its internal loadAudioFile routine does not decode actual audio. This is a release-truth defect even if the separate WAV/backend code is usable.

The FFI contains explicit deployment stubs. It must either be implemented and gated or removed from the supported surface.

The legacy litt_engine2 header contains a separate hand-written JSON parser/runtime used by old demo/test code. It duplicates the hardened JSON/runtime path and should be migrated or quarantined.

### World generation

Historical correctness punch lists are closed and WorldKit regressions pass, but WorldGen has not yet earned the low-resource quality claim. Representative generators still need deterministic output, generation-time, disk/file-count, mesh-reuse, entity/collision, connectivity, and runtime-cost measurements.

### Documentation

Documentation currently describes more capability than the green executable contract proves. Release documentation must distinguish supported, partial, experimental, and legacy paths.

## Execution order

1. #43 supported API/runtime contract
2. #44 audio truth/correctness
3. #45 renderer/GPU consolidation
4. #46 FFI deployment
5. #47 legacy runtime consolidation
6. #48 WorldGen quality/resource pass
7. #49 resource baseline
8. #50 documentation/packaging
9. rerun full clean release matrix
10. close #41 only after all release-blocking findings are fixed or explicitly isolated

For every task use: CHECK -> FIX -> VALIDATE -> IMPLEMENT -> COMPILE -> BENCHMARK where relevant -> CONSOLIDATE -> RELEASE GATE.
