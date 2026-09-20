# Agent-Skills Capability Promotion Log

This report is the cumulative engineering handoff for issue #53. Every capability update must be followed by a fresh audit entry. Later work should read this log, then verify its findings against current code and CI rather than treating old observations as automatically current.

## Update 001 - C bridge connection truthfulness

Branch: `agent-skills-capability-promotion-20260920`  
PR: #54  
Validated head: `63e0b9212c7b535c66abff0f5245c5918cf14785`

### Change

The release C bridge previously implemented `litt_connect()` as a simulated success. It now fails explicitly because no release-supported transport backend is wired to the C bridge. The state becomes ERROR, the operation returns false, and disconnect remains the explicit reset.

### Regression coverage and CI

`litt_c_tests.cpp` verifies unavailable remote connect, disconnected state, ERROR state, reset behavior, and the existing C bridge contracts. Stabilization run #171 passed all six jobs.

### Audit

Correctness and API truthfulness improved with negligible resource impact. Networking remains unavailable until a real protocol/transport contract exists.

## Update 002 - Scene hierarchy persistence v1

Branch: `agent-skills-capability-promotion-20260920`  
PR: #54

### Contract

Scene JSON v1 persists node IDs, names, parent links, position, quaternion rotation, scale, visibility, cullability, and root identity. Nodes are emitted in sorted-ID order for deterministic output. Runtime/render attachments are deliberately excluded until their own persistence contracts are defined.

Deserialization is transactional: input is parsed into a candidate scene and the live scene is replaced only after validation and hierarchy reconstruction succeed. Strict JSON parsing is used. Duplicate IDs, missing parents, cycles, malformed structures and non-finite transform data are rejected.

### Implementation updates

`litt_scene.h` now contains real serialize/deserialize behavior instead of explicit unavailable stubs. This introduced a dependency on Litt's existing dependency-free C JSON parser, not an external library.

Initial CI run #173 exposed an integration error: header-inline scene deserialization referenced `litt_json.c` symbols, but stabilization binaries did not link that C object. The C runtime, C bridge and generated-game jobs remained green. Build wiring was corrected for Linux, sanitizer and Windows paths.

The historical stabilization test still asserted that scene serialization must be unavailable. Run #176 compiled successfully after the link fix and reached 61 passing checks with one obsolete assertion failure. A transitional stabilization runner now preserves the historical suite while replacing only that obsolete scene-persistence expectation with the current contract. A dedicated `litt_scene_persistence_tests.cpp` contract suite was added for round-trip hierarchy/transform/flag/name behavior and malformed/missing-parent/cycle/non-finite rejection.

Final candidate head `1e3cb30eb3b53cc7b7c5b6fe0a1ef3cc6e52a30f` passed Stabilization run #180. PR #54 was squash-merged to main as `252ff2dd5a06b3c1da57e27f37d68212f48c0297`.

### Audit

Correctness: promoted after full CI. Failure behavior is explicit and transactional.

Architecture: remains headless and dependency-light. Reusing `litt_json.c` is preferable to introducing a C++ JSON package, though inline implementation means consumers of `litt_scene.h` that call persistence must link the JSON object. A later cleanup should move scene persistence implementation into a `.cpp` translation unit to keep header dependencies cleaner.

Compatibility: serialized format is explicitly version 1. Attachments are not silently discarded as a claimed full-scene save contract; v1 is a scene-graph persistence contract only.

Resource impact: serialization allocates an ID vector and output string proportional to scene size. Deserialization temporarily owns a second scene for transactional safety, so peak memory is roughly old scene plus candidate scene plus parsed JSON. This is intentional and bounded by input size. No graphics, network, thread or heavyweight runtime dependency was added.

Security/robustness: strict parsing and finite-number checks improve malformed-input handling. Remaining hardening includes explicit scene/node count and input-size limits for hostile/untrusted files.

## Update 003 - Engine scene persistence facade

Branch: `engine-scene-facade-20260920`  
PR: #55  
Validated head: `8622ef11bd09723885bba57cccbb4b6b0635f5c0`  
Merged main: `8f5497f8b60a3b84a2f97b819f9a1633b1acc555`

### Contract and change

`Engine::load_scene()` and `Engine::save_scene()` now use the active Scene's validated persistence-v1 contract instead of reporting unavailable. Load requires an active scene and non-empty path, rejects missing/read-failed/invalid files, and relies on transactional scene deserialization so malformed input does not mutate live state. Save serializes the active scene, writes a sibling temporary file first, validates the stream, then replaces the destination. Ordinary serialization/write failure therefore does not truncate an existing scene file.

The same update fixes an adjacent lifecycle defect: `initialize()` resets `running_` and clears stale SceneManager state before creating the default scene, making stop then reinitialize coherent rather than leaving the engine stopped with retained scenes.

### Regression coverage and CI

`litt_engine_scene_tests.cpp` covers headless initialization, active default scene creation, hierarchy setup, save/load round trip, replacement of an existing save, malformed-load non-mutation, missing-file failure, empty-save-path failure, stop state, reinitialization, running-state reset and stale-scene cleanup.

Stabilization run #182 passed all six jobs on the validated PR head. The Engine scene facade contract passed normal Linux, ASan/UBSan, and Windows/MSVC gates. C runtime, C bridge/SDK, generated-game, and the wider Windows native suite also remained green. PR #55 was therefore squash-merged rather than promoted on partial evidence.

### Fresh audit

Correctness: promoted after full cross-platform CI. Failure behavior remains explicit and malformed loads are transactional.

Portability: Linux/POSIX replacement is stronger than the current Windows remove-then-rename fallback. Windows ordinary write failure is protected, but replacement is not crash-atomic between removal and rename. A platform-specific atomic-replace helper remains a hardening opportunity.

Architecture/resource impact: dependency-free and headless-compatible. Save temporarily holds serialized JSON in memory and on disk; load holds the file buffer plus parser/candidate scene. No graphics SDK or heavyweight runtime dependency was introduced.

Security/robustness: strict scene parsing is retained. Input-size/node-count limits remain an identified hardening task. Concurrent same-path saves are outside the v1 contract.

Compatibility: attachment persistence remains intentionally outside scene JSON v1. Engine save/load is scene-graph persistence, not yet complete runtime-state persistence.

## Update 004 - Post-promotion C bridge truth audit

Base inspected: main `8f5497f8b60a3b84a2f97b819f9a1633b1acc555`

### Findings

The next highest-priority truthfulness gap is confirmed in `litt_c.cpp`. `litt_world_add_component()` currently accepts a `config_json` argument, discards it unconditionally, sets only a component membership bit, and returns success. For Mesh, Physics, Light, Camera, Script, Audio, and UI this can be mistaken for successful runtime component configuration even though no configuration is consumed or wired into the native runtime.

The C bridge quality and camera controls are also metadata-only today. `litt_engine_set_quality()` stores an enum without applying it to a renderer. `litt_engine_set_camera()` validates and stores camera state without driving a release-supported framebuffer/render path. GPU and framebuffer queries correctly remain unavailable, which makes the setters' implied runtime effect the inconsistent part of the contract.

### Risk and next implementation contract

Do not remove the lightweight metadata capability just to make the API smaller. The next slice should make the distinction explicit and testable: component membership/configuration must either be persisted and retrievable as bridge metadata or rejected when configuration cannot be honored; invalid JSON must never report success. Renderer-facing controls need capability/status semantics so callers can distinguish stored editor state from an applied renderer setting.

This work should remain allocation-light and dependency-free by reusing Litt's strict JSON parser. It must extend the packaged C SDK contract and Windows compile/test coverage before promotion.

### Next work

Implement the C bridge component/config truth contract on `c-bridge-component-contract-20260920`, add regression coverage for malformed JSON, missing entities, unsupported component values, config round trip or explicit rejection, removal cleanup, and transform invariants. Then audit quality/camera semantics before moving to asset/material attachment persistence.
