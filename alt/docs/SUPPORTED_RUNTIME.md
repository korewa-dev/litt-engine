# Supported Runtime Contract

This document defines the release-supported Litt Engine path. Interfaces outside this contract may exist for experimentation or compatibility, but they are not evidence of production readiness.

## Supported path

The primary supported path is the native generated-game runtime under `alt/src/native` plus the WorldGen/template toolchain under `alt/tools/template`.

Release CI currently gates:

- native C runtime contract tests
- generated-game validation and a 60-frame example-village run
- WorldKit and generated-prop regressions
- C bridge entity/component/transform contract tests
- C++ stabilization regressions under ASan/UBSan
- Windows native tests
- standalone compilation of every public `littcore/*.h` header
- Windows compilation of the C bridge

## Supported build smoke test

On Linux:

```sh
make -C alt/src/native test
make -C alt/src/native bin/littcli bin/littview
alt/src/native/bin/littcli validate alt/Project/example-village --frames 60
python3 alt/tools/template/tools/worldgen/test_worldkit.py
python3 alt/tools/template/tools/worldgen/test_gen_props.py
```

The repository Stabilization workflow is the cross-platform release gate.

## API status

### Release-supported

- native C runtime used by generated games
- generated-game validation/runtime path
- hardened JSON/world/OBJ contracts covered by native tests
- WorldKit deterministic generation helpers covered by regressions
- C bridge entity/component/transform state contract

### Partial

- C++ Engine facade
- scene hierarchy and runtime components
- software/native preview rendering
- physics
- audio
- scripting VM
- asset/material pipeline

A partial API may be useful, but callers must not assume features beyond the behavior covered by tests.

### Experimental or unavailable

- generic Vulkan/DX12/OpenGL/Metal renderer facade backends that return unavailable from their current initialization paths
- GPU ray tracing claims
- FFI world deployment stubs
- networking interfaces without a release contract
- Python/C#/Lua integrations not exercised by the Stabilization matrix
- legacy `litt_engine2.h` JSON runtime

## Compatibility rule

Unsupported or unavailable operations must fail explicitly. They must not return fabricated GPU data, fake entity IDs, mock framebuffers, or successful no-op mutations.

## Resource rule

The supported path should remain small and dependency-light. New default dependencies, large fixed allocations, excessive generated assets, or substantial runtime memory increases require measurement and justification.

## Promotion rule

A partial/experimental subsystem becomes release-supported only after it has:

1. an explicit behavioral contract;
2. regression tests for success and failure cases;
3. Windows/Linux compile coverage where applicable;
4. sanitizer coverage for memory-unsafe code paths;
5. documentation matching the tested behavior.
