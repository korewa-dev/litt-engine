//! Engine Modules -- Phase 9 Implementation
//!
//! Core engine systems that integrate all lower-level crates:
//! - Input (litt-input): keyboard, mouse, gamepad
//! - Audio (litt-audio): sound playback and mixing
//! - UI (litt-ui): debug HUD, overlays, text rendering
//! - Profiler (litt-profiler): frame timing, GPU/CPU sync
//! - Scene (litt-scene): scene graph, hierarchy, loading
//! - Config (litt-config): settings, presets, persistence
//! - Game Loop (src/game_loop.rs): fixed timestep with sub-stepping
//! - App (src/app.rs): integration of all systems

// =============================================================================
// Phase 9: Engine Modules [ IMPLEMENTED]
// =============================================================================

// All modules are exported from lib.rs and main.rs
