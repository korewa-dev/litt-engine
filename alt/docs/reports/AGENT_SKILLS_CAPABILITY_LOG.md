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

CI now explicitly gates the dedicated scene persistence contract on normal Linux, ASan/UBSan and Windows builds. Current candidate head after gate wiring: `7ed7ef5fe56ea344425514ae310fc774433996be`. This head is not promoted until its workflow completes green.

### Audit

Correctness: materially improved, but promotion is pending CI. Failure behavior is explicit and transactional.

Architecture: remains headless and dependency-light. Reusing `litt_json.c` is preferable to introducing a C++ JSON package, though inline implementation means consumers of `litt_scene.h` that call persistence must link the JSON object. A later cleanup should move scene persistence implementation into a `.cpp` translation unit to keep header dependencies cleaner.

Compatibility: serialized format is explicitly version 1. Attachments are not silently discarded as a claimed full-scene save contract; v1 is a scene-graph persistence contract only.

Resource impact: serialization allocates an ID vector and output string proportional to scene size. Deserialization temporarily owns a second scene for transactional safety, so peak memory is roughly old scene plus candidate scene plus parsed JSON. This is intentional and bounded by input size. No graphics, network, thread or heavyweight runtime dependency was added.

Security/robustness: strict parsing and finite-number checks improve malformed-input handling. Remaining hardening includes explicit scene/node count and input-size limits for hostile/untrusted files.

### Next work

First require the current CI head to pass and fix any platform/sanitizer failures. Then wire `Engine::load_scene()` and `Engine::save_scene()` to the validated scene persistence contract with atomic file replacement/failure tests. After that, audit C bridge component/config truthfulness before promoting additional subsystems.
