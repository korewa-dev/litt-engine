# Contributing to Litt Engine

Thank you for wanting to contribute. Litt Engine is a small, focused project  every PR is expected to earn its place.

## Before You Start

1. **Read ROADMAP.md.** Your feature may already be planned, or it may conflict with an existing direction.
2. **Check open issues.** Don't duplicate work.
3. **Open an issue first** for non-trivial changes. Small fixes (typos, one-line corrections) can go straight to PR.

## Development Setup

Run these commands to build and run:
- git clone https://github.com/korewa-dev/litt-engine.git
- cd litt-engine
- cargo build --release
- cargo run --release

For DX12: cargo build --release --features dx12

## Code Style

- **Rust:** rustfmt + clippy --D warnings
- **GLSL/HLSL:** 4-space indent, K&R braces, trailing semicolons
- **Markdown:** 80-char line wrap where practical
- **Comments:** Explain why, not what. The code says what.

## Commit Messages

Format: <type>: <subject>

Types: feat, fix, docs, refactor, perf, test, chore

Examples:
- feat(ecs): add PhysicsBody component
- fix(renderer): fix command buffer leak on resize
- docs: update ROADMAP phase 5 status

## PR Checklist

- [ ] Branch is up to date with main
- [ ] cargo fmt and cargo clippy pass
- [ ] Tests pass (cargo test)
- [ ] Binary size target respected (< 1 MB dev build)
- [ ] Documentation updated (README, subsystem docs, ROADMAP)
- [ ] New subsystem gets its own docs/<folder>/README.md
- [ ] Changelog entry (if user-facing change)

## What Gets Rejected

- Features without a roadmap entry (unless it's a bug fix)
- New GPU/CPU fallback paths for NPU inference (existing fallbacks are deprecated, not removed)
- Bloat that pushes binary size over target
- Undocumented subsystems
- Breaking API changes without migration path

## Subsystem Documentation

Every new subsystem must have:
1. docs/<folder>/README.md  overview and file index
2. Subsystem-specific .md files for deep dives
3. Entry in docs/README.md navigation hub
4. Roadmap entry in ROADMAP.md

## Architecture

The engine structure:
- crates/math/  SIMD types (zero deps)
- crates/platform/  Window + input abstraction
- crates/vulkan/  Vulkan 1.3 backend
- crates/dx12/  DirectX 12 backend
- crates/renderer/  Command pools, render passes
- crates/pathtracer/  Ray tracing pipeline
- crates/fidelityfx/  FSR 3/4, CAS, denoisers
- crates/ecs/  Entity Component System
- shaders/  GLSL/HLSL shader source
- src/  Main crate
- docs/  Documentation
- template/  Example components + agent templates

## Questions?

Open an issue with the question label. No private DMs  keep it public.

---

*Last updated: 2026*


