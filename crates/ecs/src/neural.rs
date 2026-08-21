//! NeuralBrain component — AI model reference and state for entities.
//! Used by NeuralAISystem for NPU/GPU/CPU-driven behavior inference.

use litt_math::Vec3;
use bytemuck::{Pod, Zeroable};

/// Behavior state for NPU-driven entities
#[derive(Clone, Copy, Debug, Default, Pod, Zeroable)]
#[repr(C)]
pub struct NeuralBrain {
    /// Model ID reference
    pub model_id: u32,
    /// Current behavior state
    pub state: u32,
    /// Confidence score (0.0-1.0)
    pub confidence: f32,
    /// Last inference latency (ms)
    pub latency_ms: f32,
    /// Padding for GPU alignment
    pub _pad: [f32; 3],
}

impl NeuralBrain {
    pub fn new(model_id: u32) -> Self {
        Self { model_id, state: 0, confidence: 0.0, latency_ms: 0.0, _pad: [0.0; 3] }
    }
}

/// Movement intent — desired velocity/direction
#[derive(Clone, Copy, Debug, Default, Pod, Zeroable)]
#[repr(C)]
pub struct MovementIntent {
    pub velocity: Vec3,
    pub target: Vec3,
    pub speed: f32,
    pub padding: f32,
}

/// Combat intent — target + action queue
#[derive(Clone, Copy, Debug, Default, Pod, Zeroable)]
#[repr(C)]
pub struct CombatIntent {
    pub target_id: u32,
    pub action: u32,
    pub aggression: f32,
    pub range: f32,
}
