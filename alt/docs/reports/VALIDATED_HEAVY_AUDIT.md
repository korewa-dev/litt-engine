# Validated Heavy Audit

Audit date: 2026-09-22
Audited main SHA: `15235b43a32c1f11ecee828670b8a55c3d721be2`

## Audit rule

This audit uses:

`understand -> specify -> test -> inspect -> validate -> audit -> repair`

A suspicious implementation is not a repair item by itself. A repair item appears below only when current code, the live support contract, tests, CI, or another current repository-owned contract establishes that the behavior is wrong, misleading, or insufficiently gated.

GPU hardware certification is intentionally allowed to remain on hold.

## Baseline

The audited SHA passed Stabilization run 35777547426. All 13 jobs completed successfully:

- GCC compiler/runtime matrix
- Clang compiler/runtime matrix
- Windows/MSVC
- ASan/UBSan stabilization
- ThreadSanitizer concurrency contract
- native C runtime
- C bridge contract
- networking contract
- generated-game contract
- WorldGen cross-OS comparison
- GPU/software renderer contract
- core stabilization
- repository hygiene

This is strong release evidence, but green CI does not prove behavior that is not covered by those tests.

## Result

Validated findings:

- RED: 1
- ORANGE: 3
- GPU physical certification: HOLD by policy, not a defect

No other candidate inspected during this run was promoted into the repair guide without a current contract establishing the expected behavior.

## RED-1: `ray_aabb` returns the ray far bound for an origin inside the box

Severity: RED
Area: canonical C++ math / collision query
Affected path: `alt/src/native/littcore/litt_math.h`

### Evidence

The current implementation computes the slab entry and exit distances as `tmin` and `tmax`, then selects:

```cpp
float t = tmin > r.t_min ? tmin : r.t_max;
```

The fallback incorrectly selects the ray's global far bound rather than the computed AABB exit distance.

Concrete reproduction:

- box: min `(-1,-1,-1)`, max `(1,1,1)`
- ray origin: `(0,0,0)`
- ray direction: `(1,0,0)`
- ray range: `[0,10]`

The slab calculation produces entry `-1` and exit `1`. Because entry is behind the ray minimum, current code selects `r.t_max == 10`, reports a hit at `(10,0,0)`, and therefore returns a point outside the box. The actual forward box exit is `t == 1`.

The existing ray/AABB regression in `alt/src/native/tests.cpp` only covers a ray that starts outside the box. That test takes the entry-distance branch and cannot detect this defect. The current Stabilization workflow does not build that old `tests.cpp` suite as its canonical engine contract.

### Why this is release-relevant

`litt_math.h` is part of the packaged public C++ SDK and is reachable through the canonical `litt.h` surface. This is executable supported code, not historical or research-only code.

### Required validation

Add a currently gated regression before changing the implementation. The regression must prove:

1. outside-origin entry remains correct;
2. inside-origin ray returns the computed box exit;
3. a ray whose `t_max` ends before that exit reports no hit;
4. an axis-parallel ray outside a slab reports no hit.

Keep any zero-direction-ray policy out of this patch unless it is separately specified and tested.

## ORANGE-1: C bridge silently accepts component configuration it does not apply

Severity: ORANGE
Area: versioned C bridge
Affected paths:
- `alt/src/native/litt_c.cpp`
- `alt/src/native/litt_c.h`
- `alt/src/native/litt_c_tests.cpp`

### Evidence

Current `litt_world_add_component` starts with:

```cpp
(void)config_json;
```

and then sets only a bit in `EntityRecord::components` before returning true.

`EntityRecord` contains the entity description plus the component bitmap. There is no per-type Mesh, Physics, Light, Camera, Script, Audio, or UI configuration state in the C bridge record.

The current test passes `"{}"` to a Mesh add and checks only the presence bit. It does not test meaningful configuration, malformed configuration, or configuration effects.

The live support contract explicitly includes the versioned C bridge for entity/component/transform use. Issue #53 also requires unavailable behavior to fail explicitly and forbids successful no-ops. Most importantly, `alt/docs/reports/AGENT_SKILLS_CAPABILITY_LOG.md` explicitly identifies this exact component/config truthfulness check as the next audit after the previous green merge.

### Conservative contract

Do not invent Mesh/Physics/Light/etc. JSON schemas in this repair.

Preserve the currently implemented presence-only component contract, but make it truthful:

- `config_json == nullptr`, an empty string, or a strict empty JSON object may represent default/no configuration;
- any meaningful object with fields, malformed JSON, array, scalar, or other unsupported configuration must fail and must not set the component bit;
- Transform remains configured through the existing transform setter/getter functions;
- the header must state that non-transform C-bridge component records are presence-only until a typed configuration contract is separately promoted.

This is a narrow truthfulness repair, not a component-system rewrite.

## ORANGE-2: C bridge release evidence is weaker than the repository's own promotion rule

Severity: ORANGE
Area: CI/release gate
Affected path: `.github/workflows/stabilization.yml`

### Evidence

The current workflow:

- runs `litt_c_tests.cpp` on Linux in the normal C-bridge job;
- builds the external C consumer on Linux;
- on Windows, only compiles `litt_c.cpp` and does not run `litt_c_tests.cpp`;
- the ASan/UBSan stabilization job does not build or run `litt_c.cpp + litt_c_tests.cpp`.

The authoritative promotion rule in `alt/docs/SUPPORTED_RUNTIME.md` requires Windows/Linux coverage where applicable and sanitizer coverage for memory-unsafe paths.

Because the C bridge is a release-supported ABI implemented in C++, compile-only Windows evidence and no sanitizer run leave a real proof gap even though the normal Linux bridge job is green.

### Required validation

Add:

- ASan/UBSan build and execution of the C bridge contract on Linux;
- Windows build and execution of the C bridge contract, linked with the existing JSON object and required Windows system libraries.

Do not add a Windows package archive requirement. The current release contract explicitly does not claim a Windows-specific SDK archive format.

## ORANGE-3: live canonical core README contains uncompilable API examples

Severity: ORANGE
Area: active developer documentation
Affected path: `alt/src/native/littcore/README.md`

### Evidence

This is a live README beside the canonical C++ core, not an archival roadmap.

Examples use API names that do not exist in the current public headers:

- `litt::Quatf`
- `litt::Rayf`
- `litt::HitInfof`
- `litt::math::ray_aabb`
- `litt::math::ray_triangle`
- `Mat4f::rotation_y`
- `World::create_entity`
- `World::add_component`
- `World::get_component`
- `World::has_component`

Current code instead exposes `Quat`, `Ray`, `HitInfo`, namespace-level `ray_aabb/ray_triangle`, `Mat4::rot_y`, and ECS methods `create/add/get/has`.

This documentation can send a C++ SDK consumer directly into compile failures.

### Required validation

Update examples to the actual current API and compile the resulting quick-start/ECS/math snippets against the packaged SDK before merging.

## Explicitly validated as non-repair items

The following were inspected and deliberately excluded from the repair guide:

### GPU hardware certification

Physical GPU certification remains on hold and is explicitly outside the current software release. No repair is required.

### Historical GPU/backend documents

Historical/research files describing Vulkan, DX12, vendors, or old roadmap states are not defects merely because the current release supports only software rendering. The authoritative support contract already marks accelerated backends unavailable.

### GitHub repository description

The repository description differs from the exact certified release surface, but repository metadata alone does not establish that it is intended to be the release contract. It is not a code repair item in this audit.

### `litt_world.cpp`

The old C++ world translation unit contains patterns worth revisiting if it is promoted, but the current native library builds `litt_world.c`, not `litt_world.cpp`. It is not part of the current canonical release link path, so this audit does not prescribe a repair.

### C bridge world save/load versus bridge entity records

The bridge-local entity map and `WorldManager` persistence are separate today. That is suspicious, but the current release contract does not define persistence semantics for the C bridge entity map. Without that contract, this audit does not invent a repair.

### C bridge quality/camera controls

Those controls are not part of the supported C-bridge release boundary, and the bridge framebuffer/render path already reports unavailable. No repair is prescribed here.

### Asset import options / async switch

The supported asset contract is synchronous import/reimport/metadata. The existence of currently unused option/async surfaces is not enough to classify a release defect without a stronger public contract.

## Unsafe repair branch warning

Branch `audit-c-bridge-components-20260922` is based on the audited main SHA and changes only `alt/src/native/litt_c.cpp`, but the diff is +26 / -305 lines, 331 total changes.

That branch must not be merged or cherry-picked. The intended C-bridge truthfulness repair is small; deleting most of `litt_c.cpp` is outside scope.

The older `c-bridge-component-contract-20260920` branch is also stale and heavily diverged from current main. It must not be used as a repair base.

## Merge readiness

The audited SHA is CI-green but does not receive final heavy-audit signoff because RED-1 is a confirmed correctness defect in the canonical public C++ math surface.

After the validated repairs are implemented, the complete Stabilization workflow must pass on the exact merged main SHA. GPU physical certification may remain on hold.
