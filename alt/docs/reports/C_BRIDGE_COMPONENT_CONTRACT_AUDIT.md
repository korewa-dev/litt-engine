# C Bridge Component Contract Audit

Branch: `c-bridge-component-contract-20260920`
Base: main `8f5497f8b60a3b84a2f97b819f9a1633b1acc555`

## Implemented

The C bridge no longer discards component configuration while reporting success. `litt_world_add_component` now accepts only strict JSON objects, normalizes null or empty configuration to `{}`, stores the exact validated configuration as lightweight bridge metadata, and rejects malformed JSON, non-object JSON, missing entities, and component values outside the public enum.

A new `litt_world_get_component_config` API makes the metadata contract observable. It supports a size query, bounded copy including the terminating NUL, and explicit zero return for missing/invalid state. Removing a component also removes its stored configuration. Transform remains non-removable.

## Regression coverage

`litt_c_tests.cpp` now covers configuration round trip, required buffer sizing, undersized-buffer failure without overwrite, malformed JSON rejection without membership mutation, non-object rejection, unsupported enum rejection, missing-entity rejection, null configuration normalization, removal cleanup, and position/rotation/scale finite-value invariants.

## Fresh audit

Correctness: the previous successful no-op configuration path has been removed. Configuration is explicitly metadata, not a claim that Mesh/Physics/Light/Camera/Script/Audio/UI runtime systems have been instantiated.

Architecture: uses the existing dependency-free strict JSON parser and `std::string`/`unordered_map`; no graphics SDK, network stack, thread, or heavyweight dependency was introduced.

Resource impact: one string plus hash-map entry per configured component. This is proportional to caller-supplied configuration size. A future hostile-input hardening pass should impose a maximum configuration length.

Compatibility: existing callers that passed `{}` or null continue to work. Callers that passed malformed JSON, JSON arrays/scalars, or out-of-range component values now correctly receive failure. The new getter is additive to the C ABI.

Remaining risk: component metadata is not yet persisted by `litt_world_save`, because C-bridge entity records are currently distinct from WorldManager persistence. That boundary must be made explicit or unified before claiming complete C-bridge world persistence.

Renderer truth gap: quality and camera setters remain stored editor metadata with no release-supported renderer application. This should be the next truthfulness slice after this component contract is CI-green.

CI status: implementation and tests committed, full Stabilization evidence pending. Do not promote or merge until Linux C bridge/SDK, Windows, and the rest of the matrix are green.
