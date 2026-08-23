//! NeuralAISystem -- NPU/GPU/CPU-driven AI behavior inference.
//! Uses the litt-ai backend to run inference on entities with NeuralBrain.

use crate::{World, Entity, System};
use crate::neural::{NeuralBrain, MovementIntent, CombatIntent};
use litt_math::Vec3;
use litt_ai::{AIContext, Model};

/// NeuralAI system -- runs AI inference to drive entity behavior
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
            // Read-only pass: copy brain state out so we can mutate afterwards.
            let info = world
                .get_component::<NeuralBrain>(entity)
                .map(|b| (b.model_id, b.state as f32, b.confidence));
            let Some((model_id, state, confidence)) = info else { continue };

            // Inference pass (no world borrows held).
            let latency = match self.models.get(&model_id) {
                Some(model) => {
                    let input = litt_ai::Tensor::from_floats(
                        &[state, confidence, 0.0, 0.0],
                        litt_ai::Shape::new(&[1, 4]),
                    );
                    match self.context.run_auto(model, &[input]) {
                        Ok(result) => Some(result.latency_ms),
                        Err(_) => None,
                    }
                }
                None => None,
            };

            // Write-back pass.
            let speed = match latency {
                Some(l) => 2.0 + l * 0.1,
                None => 1.0,
            };
            if let Some(movement) = world.get_component_mut::<MovementIntent>(entity) {
                movement.speed = speed;
            }
            if let (Some(l), Some(brain)) = (latency, world.get_component_mut::<NeuralBrain>(entity)) {
                brain.latency_ms = l;
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

/// CombatAISystem -- NPU-driven combat behavior
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
            let has_brain = world.get_component::<NeuralBrain>(entity).is_some();
            if has_brain {
                if let Some(combat) = world.get_component_mut::<CombatIntent>(entity) {
                    // Simple combat AI: escalate aggression toward a cap
                    combat.aggression = (combat.aggression * 0.9 + 0.5).min(1.0);
                }
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
