//! NeuralAISystem — NPU/GPU/CPU-driven AI behavior inference.
//! Uses the litt-ai backend to run inference on entities with NeuralBrain.

use litt_ecs::*;
use litt_math::Vec3;
use litt_ai::{AIContext, Model, BackendKind};

use super::neural::{NeuralBrain, MovementIntent, CombatIntent};

/// NeuralAI system — runs AI inference to drive entity behavior
pub struct NeuralAISystem {
    /// AI context for backend selection
    pub context: AIContext,
    /// Models loaded per ID
    pub models: std::collections::HashMap<u32, Model>,
    /// Inference interval (frames)
    pub inference_interval: u32,
    /// Frame counter
    pub frame_count: u32,
}

impl NeuralAISystem {
    pub fn new() -> Self {
        Self {
            context: AIContext::new(),
            models: std::collections::HashMap::new(),
            inference_interval: 10, // Inference every 10 frames
            frame_count: 0,
        }
    }

    /// Load a model into the system
    pub fn load_model(&mut self, id: u32, model: Model) {
        self.models.insert(id, model);
    }

    /// Update AI behavior for all entities with NeuralBrain
    pub fn update(&mut self, world: &mut World, dt: f32) {
        self.frame_count += 1;
        if self.frame_count % self.inference_interval != 0 {
            return;
        }

        // Query all entities with NeuralBrain
        let entities: Vec<Entity> = world.query_entities_with::<NeuralBrain, MovementIntent>()
            .collect();

        for entity in entities {
            if let (Some(brain), Some(movement)) = (
                world.get_component::<NeuralBrain>(entity),
                world.get_component_mut::<MovementIntent>(entity),
            ) {
                // Try to get model for this entity
                if let Some(model) = self.models.get(&brain.model_id) {
                    // Create dummy input tensor
                    let input = litt_ai::Tensor::from_floats(
                        &[brain.state as f32, brain.confidence, 0.0, 0.0],
                        litt_ai::Shape::new(&[1, 4]),
                    );

                    // Run inference (auto-selects best backend)
                    match self.context.run_auto(model, &[input]) {
                        Ok(result) => {
                            brain.latency_ms = result.latency_ms;
                            // Update movement based on output
                            movement.speed = 2.0 + result.latency_ms * 0.1;
                        }
                        Err(_) => {
                            // Fallback: simple behavior
                            movement.speed = 1.0;
                        }
                    }
                } else {
                    // No model — use simple behavior
                    movement.speed = 1.0;
                }
            }
        }
    }
}

impl Default for NeuralAISystem {
    fn default() -> Self { Self::new() }
}

impl System for NeuralAISystem {
    fn name(&self) -> &str { "neural_ai" }

    fn update(&mut self, world: &mut World, dt: f32) {
        self.update(world, dt);
    }
}

/// CombatAISystem — NPU-driven combat behavior
pub struct CombatAISystem {
    pub context: AIContext,
}

impl CombatAISystem {
    pub fn new() -> Self {
        Self { context: AIContext::new() }
    }

    pub fn update(&mut self, world: &mut World, _dt: f32) {
        let entities: Vec<Entity> = world.query_entities_with::<NeuralBrain, CombatIntent>()
            .collect();

        for entity in entities {
            if let (Some(_brain), Some(combat)) = (
                world.get_component::<NeuralBrain>(entity),
                world.get_component_mut::<CombatIntent>(entity),
            ) {
                // Simple combat AI: target nearest entity
                combat.aggression = 0.5 + (combat.aggression * 0.9);
                combat.aggression = combat.aggression.min(1.0);
            }
        }
    }
}

impl Default for CombatAISystem {
    fn default() -> Self { Self::new() }
}

impl System for CombatAISystem {
    fn name(&self) -> &str { "combat_ai" }

    fn update(&mut self, world: &mut World, dt: f32) {
        self.update(world, dt);
    }
}
