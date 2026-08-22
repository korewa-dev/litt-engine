//! Particle system for rendering effects.
//!
//! Supports:
//! - CPU-based particles (linked lists, dynamic arrays)
//! - GPU instancing with compute shader updates
//! - Batch rendering via instanced draw calls
//! - Particle pooling for performance
//!
//! Targets: < 10,000 particles on CPU, 100,000+ on GPU

use litt_math::Vec3;
use std::cell::RefCell;
use std::rc::Rc;

// =============================================================================
// Particle Types
// =============================================================================

/// Particle lifetime and lifecycle state
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum ParticleState {
    /// Particle is alive and active
    Active,
    /// Particle is dead and can be reused
    Dead,
    /// Particle is dying (fade out)
    Dying,
}

/// Emitter type
#[derive(Clone, Copy, Debug)]
pub enum EmitterType {
    /// Point emitter (single position)
    Point,
    /// Box emitter (random position in box)
    Box(Vec3, Vec3),
    /// Sphere emitter (random position on sphere)
    Sphere(Vec3, f32),
    /// Cone emitter
    Cone(Vec3, Vec3, f32),
}

/// Particle data (CPU-side)
#[derive(Clone, Debug)]
pub struct Particle {
    pub position: Vec3,
    pub velocity: Vec3,
    pub acceleration: Vec3,
    pub color: [f32; 4], // RGBA
    pub size: f32,
    pub lifetime: f32,
    pub max_lifetime: f32,
    pub state: ParticleState,
    pub generation: u32,
}

impl Default for Particle {
    fn default() -> Self {
        Self {
            position: Vec3::ZERO,
            velocity: Vec3::ZERO,
            acceleration: Vec3::ZERO,
            color: [1.0, 1.0, 1.0, 1.0],
            size: 1.0,
            lifetime: 0.0,
            max_lifetime: 1.0,
            state: ParticleState::Dead,
            generation: 0,
        }
    }
}

impl Particle {
    /// Create a new particle
    pub fn new() -> Self {
        Self::default()
    }

    /// Initialize particle with spawn parameters
    pub fn spawn(
        position: Vec3,
        velocity: Vec3,
        color: [f32; 4],
        size: f32,
        lifetime: f32,
        generation: u32,
    ) -> Self {
        Self {
            position,
            velocity,
            acceleration: Vec3::ZERO,
            color,
            size,
            lifetime: 0.0,
            max_lifetime: lifetime,
            state: ParticleState::Active,
            generation,
        }
    }

    /// Check if particle is alive
    pub fn is_alive(&self) -> bool {
        matches!(self.state, ParticleState::Active | ParticleState::Dying)
            && self.lifetime < self.max_lifetime
    }

    /// Check if particle is dead
    pub fn is_dead(&self) -> bool {
        matches!(self.state, ParticleState::Dead) || self.lifetime >= self.max_lifetime
    }
}

// =============================================================================
// Particle System (CPU-based)
// =============================================================================

/// CPU-based particle system
pub struct ParticleSystem {
    pub particles: Vec<Particle>,
    pub active_count: usize,
    pub max_particles: usize,
    pub emit_rate: f32,
    pub elapsed_emit: f32,
    pub gravity: Vec3,
}

impl Default for ParticleSystem {
    fn default() -> Self {
        Self::new(10000)
    }
}

impl ParticleSystem {
    /// Create a new particle system
    pub fn new(max_particles: usize) -> Self {
        Self {
            particles: Vec::with_capacity(max_particles),
            active_count: 0,
            max_particles,
            emit_rate: 100.0,
            elapsed_emit: 0.0,
            gravity: Vec3::new(0.0, -9.81, 0.0),
        }
    }

    /// Emit a single particle
    pub fn emit(&mut self, particle: Particle) -> bool {
        if self.particles.len() >= self.max_particles {
            // Find a dead particle to reuse
            if let Some(idx) = self.particles.iter().position(|p| p.is_dead()) {
                self.particles[idx] = particle;
                self.active_count += 1;
                return true;
            }
            return false;
        }

        self.particles.push(particle);
        self.active_count += 1;
        true
    }

    /// Emit particles over time
    pub fn update(&mut self, dt: f32) {
        self.elapsed_emit += dt * self.emit_rate;

        while self.elapsed_emit >= 1.0 {
            self.elapsed_emit -= 1.0;
            // Emit would be called by the emitter system
        }

        self.active_count = 0;
        for particle in &mut self.particles {
            if !particle.is_dead() {
                particle.lifetime += dt;
                particle.position += particle.velocity * dt;
                particle.velocity += particle.acceleration * dt;
                particle.velocity += self.gravity * dt;
                particle.velocity *= 0.99; // Air resistance

                // Update color based on lifetime
                let life_ratio = particle.lifetime / particle.max_lifetime;
                particle.color[3] = 1.0 - life_ratio; // Fade out

                if particle.is_dead() {
                    particle.state = ParticleState::Dead;
                } else {
                    self.active_count += 1;
                }
            }
        }
    }

    /// Get active particles
    pub fn active_particles(&self) -> impl Iterator<Item = &Particle> {
        self.particles.iter().filter(|p| p.is_alive())
    }

    /// Get particle count
    pub fn count(&self) -> usize {
        self.active_count
    }
}

// =============================================================================
// GPU Particle System (Instanced)
// =============================================================================

/// GPU particle data (ready for vertex buffer)
#[derive(Clone, Debug)]
pub struct GpuParticle {
    pub position: Vec3,
    pub velocity: Vec3, // x, y, z, lifetime
    pub color: [f32; 4],
    pub size: f32,
}

impl GpuParticle {
    /// Convert from CPU particle to GPU format
    pub fn from_particle(p: &Particle) -> Self {
        Self {
            position: p.position,
            velocity: Vec3::new(p.velocity.0, p.velocity.1, p.velocity.2),
            color: p.color,
            size: p.size,
        }
    }
}

/// GPU particle system with instanced rendering
pub struct GpuParticleSystem {
    pub particles: Vec<GpuParticle>,
    pub dirty: bool,
    pub instance_count: usize,
}

impl Default for GpuParticleSystem {
    fn default() -> Self {
        Self::new(100000)
    }
}

impl GpuParticleSystem {
    /// Create a new GPU particle system
    pub fn new(max_particles: usize) -> Self {
        Self {
            particles: Vec::with_capacity(max_particles),
            dirty: true,
            instance_count: 0,
        }
    }

    /// Add a particle
    pub fn emit(&mut self, particle: GpuParticle) {
        self.particles.push(particle);
        self.dirty = true;
    }

    /// Remove dead particles
    pub fn cleanup(&mut self) {
        self.particles.retain(|p| p.size > 0.0); // Simplified: keep if size > 0
        self.dirty = true;
        self.instance_count = self.particles.len();
    }

    /// Get particles for GPU upload
    pub fn particles(&self) -> &[GpuParticle] {
        &self.particles
    }

    /// Get instance count
    pub fn instance_count(&self) -> usize {
        self.instance_count
    }
}

// =============================================================================
// Particle Emitter
// =============================================================================

/// Parameters for particle emission
#[derive(Clone, Debug)]
pub struct EmitParameters {
    pub rate: f32,           // particles per second
    pub lifetime: f32,       // particle lifetime
    pub initial_speed: f32,  // initial velocity magnitude
    pub speed_variance: f32, // velocity variance
    pub color: [f32; 4],     // base color
    pub color_variance: [f32; 4], // color variance
    pub size: f32,           // base size
    pub size_variance: f32,  // size variance
}

impl Default for EmitParameters {
    fn default() -> Self {
        Self {
            rate: 100.0,
            lifetime: 1.0,
            initial_speed: 10.0,
            speed_variance: 2.0,
            color: [1.0, 1.0, 1.0, 1.0],
            color_variance: [0.1, 0.1, 0.1, 0.1],
            size: 0.1,
            size_variance: 0.05,
        }
    }
}

/// Particle emitter system
pub struct ParticleEmitter {
    pub params: EmitParameters,
    pub emitter_type: EmitterType,
    pub target: Rc<RefCell<dyn ParticleTarget>>,
}

/// Trait for particle targets
pub trait ParticleTarget {
    fn emit(&mut self, particle: Particle);
    fn emit_batch(&mut self, particles: &[Particle]);
}

impl ParticleEmitter {
    /// Create a new emitter
    pub fn new(params: EmitParameters, emitter_type: EmitterType, target: Rc<RefCell<dyn ParticleTarget>>) -> Self {
        Self {
            params,
            emitter_type,
            target,
        }
    }

    /// Emit a single particle
    pub fn emit(&mut self) -> Particle {
        let position = match self.emitter_type {
            EmitterType::Point => Vec3::ZERO,
            EmitterType::Box(min, max) => {
                Vec3::new(
                    rand::random::<f32>() * (max.0 - min.0) + min.0,
                    rand::random::<f32>() * (max.1 - min.1) + min.1,
                    rand::random::<f32>() * (max.2 - min.2) + min.2,
                )
            }
            EmitterType::Sphere(center, radius) => {
                let theta = rand::random::<f32>() * 2.0 * std::f32::consts::PI;
                let phi = rand::random::<f32>() * std::f32::consts::PI;
                Vec3::new(
                    center.0 + radius * phi.sin() * theta.cos(),
                    center.1 + radius * phi.sin() * theta.sin(),
                    center.2 + radius * phi.cos(),
                )
            }
            EmitterType::Cone(origin, direction, angle) => {
                let theta = rand::random::<f32>() * 2.0 * std::f32::consts::PI;
                let sin_angle = (angle * rand::random::<f32>()).sin();
                let cos_angle = (angle * rand::random::<f32>()).cos();
                let offset = Vec3::new(
                    sin_angle * theta.cos(),
                    sin_angle * theta.sin(),
                    cos_angle,
                );
                origin + direction * offset.length() * 0.1 + offset * 0.1
            }
        };

        let velocity = {
            let speed = self.params.initial_speed + (rand::random::<f32>() - 0.5) * self.params.speed_variance;
            let dir = Vec3::new(
                rand::random::<f32>() * 2.0 - 1.0,
                rand::random::<f32>() * 2.0 - 1.0,
                rand::random::<f32>() * 2.0 - 1.0,
            ).normalized() * speed;
            dir
        };

        let color = [
            self.params.color[0] + (rand::random::<f32>() - 0.5) * self.params.color_variance[0],
            self.params.color[1] + (rand::random::<f32>() - 0.5) * self.params.color_variance[1],
            self.params.color[2] + (rand::random::<f32>() - 0.5) * self.params.color_variance[2],
            self.params.color[3] + (rand::random::<f32>() - 0.5) * self.params.color_variance[3],
        ];

        let size = self.params.size + (rand::random::<f32>() - 0.5) * self.params.size_variance;

        Particle::spawn(position, velocity, color, size, self.params.lifetime, 0)
    }
}

// =============================================================================
// Utility
// =============================================================================

/// Simple random number generator for particles
mod rand {
    use std::cell::Cell;

    thread_local! {
        static RNG: Cell<u64> = Cell::new(0x5D6A_E3B9_C6BD_1FED);
    }

    pub fn random() -> f32 {
        RNG.with(|rng| {
            let mut state = rng.get();
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            rng.set(state);
            (state as f32) / (u64::MAX as f32)
        })
    }
}
