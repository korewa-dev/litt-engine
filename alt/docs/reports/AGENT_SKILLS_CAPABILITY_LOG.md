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

## Update 003 - Engine scene persistence facade, implementation audit

Branch: `engine-scene-facade-20260920`  
Implementation commit: `ce16e996c47a5fc749604d2d27145800a0bda9fc`

### Contract and change

`Engine::load_scene()` and `Engine::save_scene()` now use the active Scene's validated persistence-v1 contract instead of reporting unavailable. Load requires an active scene and non-empty path, rejects missing/read-failed/invalid files, and relies on transactional scene deserialization so malformed input does not mutate live state. Save serializes the active scene, writes a sibling temporary file first, validates the stream, then replaces the destination. Ordinary serialization/write failure therefore does not truncate an existing scene file.

The same update fixes an adjacent lifecycle defect: `initialize()` resets `running_` and clears stale SceneManager state before creating the default scene, making stop then reinitialize coherent rather than leaving the engine stopped with retained scenes.

### Fresh audit

Correctness: implementation is present but not promoted yet. Dedicated Engine-facade regression tests still need to be added and CI must pass before merge.

Failure behavior: missing/empty paths and malformed scene data fail explicitly. The underlying scene transaction protects in-memory state on parse/validation failure.

Portability: Linux/POSIX rename can replace an existing destination. Windows C rename cannot reliably do so, therefore the implementation removes the old destination after the complete temporary file exists. This avoids partial files from normal write failures, but Windows replacement is not crash-atomic between remove and rename. A future platform-specific atomic-replace helper could improve this without changing the public contract.

Architecture/resource impact: remains dependency-free and headless-compatible. File IO uses standard C++ streams. Save temporarily holds serialized JSON in memory and on disk; load holds the file buffer plus the parser/candidate scene. No graphics SDK or new runtime dependency is introduced.

Security/robustness: inherits strict scene parsing. Input-size/node-count limits remain an identified hardening task. Temporary filename collision is possible if concurrent writers save the same path; concurrent same-file saves are not part of the v1 contract and should be documented/tested before claiming thread-safe persistence.

Compatibility: attachment persistence remains intentionally outside scene JSON v1. Engine save/load must not be described as complete runtime-state persistence until mesh/material/light/camera/physics attachment contracts are implemented.

### Next work

Add Engine-facade tests for headless initialize, save/load round trip, malformed-load non-mutation, missing-file failure, replacement of an existing save, and stop/reinitialize lifecycle. Gate them on Linux, sanitizer and Windows. Only then update supported-runtime documentation and merge. After promotion, audit C bridge component/config truthfulness.
