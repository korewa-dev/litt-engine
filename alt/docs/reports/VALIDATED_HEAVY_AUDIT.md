# Validated Heavy Audit

Audit date: 2026-09-23
Audited main SHA: `657455360e4f0208e7fe286a46d41f1bffc0b863`

## Method and classification rule

This audit followed:

`understand -> specify -> test -> inspect -> validate -> audit`

A suspicious implementation was promoted to RED/ORANGE only after validation against current source, the authoritative release contract, current tests, exact-main CI evidence, documentation intent, and relevant historical/archive status. Intentional limitations, explicitly unavailable backends, deferred capabilities, archival/research material, and unusual-but-correct metadata are not repair items.

GPU physical certification remains HOLD by policy and is not a defect.

## Current baseline and CI evidence

The current main SHA is the documentation-only follow-up to `15235b43a32c1f11ecee828670b8a55c3d721be2`; no engine implementation changed in that commit.

Stabilization run `35785302169` executed on exact SHA `657455360e4f0208e7fe286a46d41f1bffc0b863` and completed successfully. All 13 jobs were green: GCC and Clang compiler/runtime matrices, Windows/MSVC, ASan/UBSan stabilization, ThreadSanitizer concurrency, native C runtime, C bridge, networking, generated-game, WorldGen cross-OS, GPU/software rendering, core stabilization, and repository hygiene.

The green run is strong evidence for covered behavior. It does not disprove defects in behavior that the current gates do not exercise.

## Result

Validated findings:

- RED: 1
- ORANGE: 3
- GPU physical certification: HOLD, not a defect

No other inspected candidate was promoted into the repair guide without a current contract establishing that a change is required.

## RED-1: `ray_aabb` uses the ray far bound instead of the computed box exit for an inside-origin ray

Severity: RED

Affected path:

- `alt/src/native/littcore/litt_math.h`

### Validation evidence

The current canonical implementation computes slab entry/exit distances as `tmin` and `tmax`, then selects:

```cpp
float t = tmin > r.t_min ? tmin : r.t_max;
```

For box `[-1,1]^3`, origin `(0,0,0)`, direction `+X`, and ray interval `[0,10]`, the slab calculation gives entry `-1` and exit `1`. Since entry is before `r.t_min`, current code selects `r.t_max == 10`, reports a hit at `(10,0,0)`, and therefore returns a point outside the AABB. The correct forward intersection is the computed exit at `t == 1`.

The existing `alt/src/native/tests.cpp` ray/AABB regression starts outside the box and only proves the entry branch. Current Stabilization does not have a currently gated inside-origin regression that would expose this branch error.

The authoritative support contract includes C++17 math in the release-supported runtime, and `litt_math.h` is part of the canonical packaged C++ SDK. This is live supported code, not archival material.

### Required repair boundary

Add a currently gated regression first, then make the smallest implementation correction: select computed `tmax` when the entry is before the ray minimum. Do not change unrelated zero-direction or boundary policy without a separately specified contract.

Required regression cases:

1. outside-origin entry remains correct;
2. inside-origin ray returns the computed exit;
3. inside-origin ray clipped by `t_max` before the exit reports no hit;
4. axis-parallel ray outside a slab reports no hit.

## ORANGE-1: supported C bridge accepts component configuration that it silently ignores

Severity: ORANGE

Affected paths:

- `alt/src/native/litt_c.cpp`
- `alt/src/native/litt_c.h`
- `alt/src/native/litt_c_tests.cpp`

### Validation evidence

Current `litt_world_add_component` explicitly discards the supplied configuration:

```cpp
(void)config_json;
```

It then mutates only the component-presence bitmap and returns success. The bridge record has no per-type Mesh/Physics/Light/Camera/Script/Audio/UI configuration state. The current bridge test proves presence using `"{}"`; it does not prove meaningful configuration is applied or rejected.

The authoritative runtime contract explicitly promotes the versioned C bridge for entity/component/transform use. Repository issue #53 establishes the capability-promotion constraint that unavailable operations fail explicitly and that successful no-ops are forbidden. The current capability log separately calls out ignored `config_json` as a truthfulness target. Taken together, successful acceptance of meaningful configuration while discarding it is inconsistent with the live promoted bridge contract.

### Required repair boundary

Do not invent component schemas. Preserve the implemented presence-only contract and make it explicit and fail-closed:

- null, empty string, or a strict empty JSON object may represent default/no configuration;
- meaningful objects, malformed JSON, arrays, scalars, or other unsupported configuration must fail before the bitmap is mutated;
- Transform configuration remains through the dedicated transform API;
- document the presence-only limitation in the public bridge header.

This is a truthfulness repair, not a component-system expansion.

## ORANGE-2: C bridge promotion evidence lacks Windows execution and sanitizer execution

Severity: ORANGE

Affected path:

- `.github/workflows/stabilization.yml`

### Validation evidence

Exact-main Stabilization run `35785302169` confirms the current gate shape:

- Linux `c-bridge-contract` builds and runs `litt_c_tests.cpp` and compiles an external packaged C consumer;
- Windows `windows-native` has a `Compile C bridge` step but does not build and execute the C bridge contract test;
- `sanitizer-stabilization` builds/runs stabilization and asset/material sanitizer coverage, but not `litt_c.cpp + litt_c_tests.cpp`.

The authoritative promotion rule requires Windows/Linux coverage where applicable and sanitizer coverage for memory-unsafe promoted paths. The C bridge is a promoted C ABI implemented by C++ and JSON-backed native code. Compile-only Windows evidence plus no sanitizer execution is therefore a real release-proof gap.

### Required repair boundary

Add Windows build-and-run coverage for the bridge contract and Linux ASan/UBSan build-and-run coverage for the bridge contract. Preserve the existing external C consumer gate. Do not invent a Windows SDK archive requirement; the support contract explicitly does not claim one.

## ORANGE-3: live canonical littcore README contains uncompilable public-API examples

Severity: ORANGE

Affected path:

- `alt/src/native/littcore/README.md`

### Validation evidence

The README is colocated with the canonical C++ core and presents itself as the active quick start and API usage guide. It is not marked archival.

Current examples use names/signatures that do not exist in the current public API, including `Quatf`, `Rayf`, `HitInfof`, `litt::math::ray_aabb`, `litt::math::ray_triangle`, `Mat4f::rotation_y`, and ECS calls `create_entity/add_component/get_component/has_component`. The current API exposes `Quat`, `Ray`, `HitInfo`, namespace-level `ray_aabb/ray_triangle`, `Mat4::rot_y`, and ECS `create/add/get/has`.

Important revalidation correction: `Vec2f`, `Vec3f`, `Vec4f`, `Mat4f`, and `Aabbf` are intentional aliases in `litt_math.h`; they are not defects and must not be mechanically renamed merely because unsuffixed types also exist.

The README also uses `Transform::data`, which is not the current transform member contract. The active quick-start examples can therefore send SDK consumers into compile failures.

### Required repair boundary

Update only invalid names/signatures and semantics, retaining valid aliases where useful. Compile the final quick-start/math/ECS snippets against the packaged C++ SDK before merge.

## Validated non-issues / excluded candidates

### GPU hardware certification and accelerated backends

Physical GPU certification remains intentionally on hold. Vulkan, DX12, OpenGL, Metal, GPU RT, and named-vendor physical certification are explicitly unavailable in `SUPPORTED_RUNTIME.md`. Research or compatibility source files do not promote those backends. No repair is required merely because such files or historical descriptions exist.

### Historical GPU/backend documents

Historical/research documents are not current release contracts. Differences between those documents and the software-renderer release are not repair items unless a document presents itself as current authoritative support documentation.

### GitHub repository description

The repository description mentions accelerated APIs/vendors, but repository metadata is not identified as the authoritative release contract. `alt/docs/SUPPORTED_RUNTIME.md` is. The description is therefore not promoted to a repair task by this audit.

### `alt/src/native/littcore/SUMMARY.md`

This file contains old-style examples and historical “Files Created” language. Its status is less clearly active than the adjacent README. Because documentation intent is ambiguous and the authoritative runtime document supersedes it for release claims, this audit does not turn it into a repair task. If maintainers want it to be a live user guide, that intent should be established first and then its examples can be validated like README snippets.

### `litt_world.cpp`

The older C++ world translation unit is not the canonical release link path; the current native library builds the C world implementation. Suspicious patterns there are not release defects unless that translation unit is promoted.

### C bridge persistence semantics

The bridge-local entity map and `WorldManager` persistence are separate. The current support contract does not define persistence semantics for the bridge entity map. No repair is invented without that contract.

### C bridge renderer quality/camera controls

These are outside the promoted C-bridge entity/component/transform boundary. Their existence does not require release behavior, and unavailable rendering paths already report unavailable. No repair is prescribed.

### Asset async/options surfaces

The release contract promotes synchronous asset import/reimport/metadata and explicitly does not claim asynchronous GPU upload. Unused option/async-looking surfaces are not defects without a stronger live contract.

## Historical/branch validation

Branch `audit-c-bridge-components-20260922` currently diverges from main by one commit and changes only `alt/src/native/litt_c.cpp`, but its diff is +26/-305 (331 changes). It is not a safe repair base and must not be merged or cherry-picked for the narrow bridge truthfulness fix.

The older `c-bridge-component-contract-20260920` repair line is stale relative to current main and should not be used as a repair base.

## Revalidation after conclusions

The exact audited main SHA already completed full Stabilization successfully in run `35785302169`. The audit then re-inspected the specific gated areas relevant to the findings: current ray/AABB code and its existing regression coverage, current C bridge implementation/header/test intent, exact-main workflow job/step evidence, the authoritative support contract, issue #53 constraints, and the active littcore README.

No tests or support constraints are recommended for weakening. The defects above survive that revalidation because the green suite does not currently exercise the missing behavior/proof.

## Merge readiness

Current main is CI-green but is not heavy-audit merge-ready for final release signoff because RED-1 is a confirmed correctness defect in the canonical supported C++ math surface. The three ORANGE findings are independently validated inconsistencies/proof gaps and should be repaired without expanding project scope.

After repairs, require targeted regressions plus a fully green Stabilization run on the exact merged main SHA. GPU physical certification may remain on hold.