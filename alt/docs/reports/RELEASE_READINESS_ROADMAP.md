# Litt Engine Release Readiness

This file records the release boundary and evidence for the completed Windows/Linux software-runtime release.

The governing lifecycle remains:

`UNDERSTAND -> SPECIFY -> TEST -> IMPLEMENT -> VALIDATE -> AUDIT -> MERGE -> VALIDATE MAIN`

A branch is not a release. The final gate is the exact merged `main` SHA.

## Engine release gates

| Gate | State | Evidence |
|---|---|---|
| GCC compiler/runtime matrix | **GREEN** | native runtime, canonical C++ engine, gameplay surface and SDK consumers |
| Clang compiler/runtime matrix | **GREEN** | same supported contracts under Clang |
| Windows/MSVC | **GREEN** | public headers and supported contracts compile/run on Windows |
| ASan/UBSan | **GREEN** | scene, engine, gameplay, assets, renderer, editor, FFI and hardening suites |
| ThreadSanitizer | **GREEN** | multithreaded deferred event-queue stress contract |
| Native C runtime | **GREEN** | JSON/world/OBJ runtime and generated-game simulation |
| C bridge / SDK | **GREEN** | versioned external C consumer |
| Packaged C++ SDK | **GREEN** | canonical headers + `liblittcore.a` external game consumer |
| Reproducible SDK package | **GREEN** | deterministic archive built twice byte-for-byte, manifest, source SHA, SHA256SUMS, LICENSE |
| Generated-game path | **GREEN** | checked-in game plus fresh natural-language game generation and native validation |
| WorldGen determinism | **GREEN** | representative output compared across Linux and Windows |
| Scene persistence properties | **GREEN** | deterministic multi-seed round trips and transactional malformed-input checks |
| Parser malformed-input smoke | **GREEN** | strict JSON, bounded OBJ/MTL, deterministic malformed OBJ smoke and network frame validation |
| Allocation/overflow limits | **GREEN** | scene, software framebuffer/buffer/texture and serialization ceilings |
| Lifecycle soak | **GREEN** | repeated Engine initialize/shutdown under sanitizer-backed hardening |
| Networking | **GREEN** | bounded framed TCP, partial frames, malformed headers, reconnects and multiple clients |
| Repository hygiene | **GREEN** | compiler intermediates rejected; Wavefront OBJ source assets preserved |
| Capability truthfulness | **GREEN** | accelerated API/vendor claims fail explicitly rather than fabricate support |

## Complete release surface

The Windows/Linux release is complete for the surface defined by `SUPPORTED_RUNTIME.md`:

- generated games and WorldGen;
- C and C++ SDK game development;
- scene/persistence and Engine lifecycle;
- software rendering, textures, render passes, UI, terrain, particles, culling, LOD and lighting;
- bounded physics, audio, scripting and networking;
- assets/materials, serialization, profiler, input/events/memory;
- portable editor core;
- Dither3D CPU processing and Dither C ABI;
- validated precompiled shader artifacts.

The release deliberately has no orange "implemented but not supported" tier. A feature is either in the supported release surface or outside it.

## Non-release promotion tracks

These are not incomplete requirements of the current software release.

### Accelerated graphics and GPU ray tracing

Vulkan, DirectX 12, OpenGL, Metal, DXR and Vulkan RT are future backend promotion tracks. Their current release contract is **GREEN fail-fast/unavailable**. A backend can only be promoted after a real implementation plus physical hardware evidence. Software CI must not impersonate that evidence.

### macOS

macOS is **not a certified release platform** for this release. The release platform matrix is Windows and Linux. Adding macOS later is a platform promotion, not an unresolved Windows/Linux engine defect.

### Physical GPU certification

No vendor/model is advertised as a Litt accelerated-rendering target. Physical certification only becomes relevant after an accelerated backend is implemented and proposed for promotion.

### Desktop GUI

The portable editor core is part of this engine release. A richer Litt GUI is a separate integration/product project and is not a core-engine completion blocker.

### Repository governance

Required-branch checks and repository description are GitHub administrative settings. They should mirror this contract, but they are not runtime code. Administrative emergency/branch-protection policy remains an owner-level repository setting.

## Promotion rule

A future feature joins the supported release only after it has:

1. explicit success and failure behavior;
2. bounded resource/error semantics;
3. regression tests;
4. relevant Windows/Linux and sanitizer coverage;
5. external-consumer proof where appropriate;
6. documentation matching executable behavior;
7. successful validation on the exact merged `main` SHA.

Increase proof before increasing claims.
