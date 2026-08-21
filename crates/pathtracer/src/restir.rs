//! ReSTIR (Reservoir-Based Sampling for Importance Sampling) for light sampling.
//!
//! ReSTIR provides efficient light sampling in complex scenes by:
//! - Maintaining a reservoir of light samples
//! - Importance-weighting samples based on lighting contribution
//! - Resampling to focus on the most important samples
//! - Temporal spatio-temporal reuse across frames
//!
//! Key benefits:
//! - Reduces noise in indirect lighting
//! - Efficient handling of many lights
//! - Better convergence than naive light sampling
//! - Supports both direct and indirect lighting

use litt_math::*;
use crate::scene::{Light, Scene};
use crate::rng::Rng;

/// Reservoir entry for a single light sample
#[derive(Clone, Copy, Debug)]
pub struct ReservoirEntry {
    /// Light index
    pub light_index: u32,
    /// Sample point on light (in world space)
    pub light_sample: Vec3,
    /// Direction from surface to light sample
    pub direction: Vec3,
    /// Weight of this sample (importance weight)
    pub weight: f32,
    /// PDF of sampling this light
    pub pdf: f32,
}

impl Default for ReservoirEntry {
    fn default() -> Self {
        Self {
            light_index: 0,
            light_sample: Vec3::ZERO,
            direction: Vec3::Z,
            weight: 0.0,
            pdf: 1.0,
        }
    }
}

/// ReSTIR reservoir for light sampling
#[derive(Debug)]
pub struct Reservoir {
    /// Current best sample
    pub entry: ReservoirEntry,
    /// Sum of all weights
    pub weight_sum: f32,
    /// Number of samples seen
    pub sample_count: u32,
}

impl Default for Reservoir {
    fn default() -> Self {
        Self {
            entry: ReservoirEntry::default(),
            weight_sum: 0.0,
            sample_count: 0,
        }
    }
}

impl Reservoir {
    /// Create a new empty reservoir
    pub fn new() -> Self {
        Self::default()
    }

    /// Initialize with a single light sample
    pub fn init(light_index: u32, light_sample: Vec3, direction: Vec3, weight: f32, pdf: f32) -> Self {
        Self {
            entry: ReservoirEntry {
                light_index,
                light_sample,
                direction: direction.normalized(),
                weight,
                pdf: pdf.max(1e-7),
            },
            weight_sum: weight,
            sample_count: 1,
        }
    }

    /// Update reservoir with a new sample using stochastic reselection
    pub fn update(&mut self, light_index: u32, light_sample: Vec3, direction: Vec3, weight: f32, pdf: f32) {
        let pdf = pdf.max(1e-7);
        
        // Stochastic reselection
        let r = (self.sample_count as f32 / (self.sample_count + 1.0) as f32)
            * self.weight_sum / (self.weight_sum + weight);
        
        if r > 0.0 && (0.0..1.0).contains(&r) {
            // Reselect existing sample
            let mut rng = Rng::new((self.sample_count as u64).wrapping_mul(0x5D588B65));
            let pick = (rng.next_f32() * (self.sample_count + 1) as f32) as u32;
            
            if pick >= self.sample_count {
                // Pick new sample
                self.entry = ReservoirEntry {
                    light_index,
                    light_sample,
                    direction: direction.normalized(),
                    weight,
                    pdf,
                };
            }
        }
        
        self.weight_sum += weight;
        self.sample_count += 1;
    }

    /// Get the current best sample
    pub fn sample(&self) -> &ReservoirEntry {
        &self.entry
    }

    /// Get the importance weight
    pub fn importance(&self) -> f32 {
        if self.sample_count == 0 {
            return 0.0;
        }
        self.entry.weight / self.weight_sum
    }
}

/// ReSTIR context for a single pixel
#[derive(Debug)]
pub struct ReSTIRContext {
    /// Current reservoir
    pub reservoir: Reservoir,
    /// Number of taps (spatial samples)
    pub num_taps: u32,
    /// Temporal reuse flag
    pub temporal_reuse: bool,
}

impl Default for ReSTIRContext {
    fn default() -> Self {
        Self {
            reservoir: Reservoir::new(),
            num_taps: 3,
            temporal_reuse: true,
        }
    }
}

impl ReSTIRContext {
    /// Create a new ReSTIR context
    pub fn new(num_taps: u32, temporal_reuse: bool) -> Self {
        Self {
            num_taps,
            temporal_reuse,
            ..Default::default()
        }
    }

    /// Initialize reservoir with a light sample
    pub fn init(&mut self, light_index: u32, light: &Light, point: Vec3, normal: Vec3, rng: &mut Rng) {
        // Sample a point on the light
        let light_sample = light.sample_point(rng);
        let direction = (light_sample - point).normalized();
        
        // Compute lighting weight
        let dist_sq = (light_sample - point).length_squared();
        let dist = dist_sq.sqrt();
        let cos_normal = normal.dot(direction).abs();
        let cos_light = normal.dot((point - light_sample).normalized()).abs();
        
        let weight = if dist_sq > 1e-6 && cos_normal > 1e-7 && cos_light > 1e-7 {
            light.intensity * cos_normal * cos_light / (dist * dist)
        } else {
            0.0
        };
        
        let pdf = 1.0 / (4.0 * std::f32::consts::PI * light.radius * light.radius).max(1e-7);
        
        self.reservoir = Reservoir::init(light_index, light_sample, direction, weight, pdf);
    }

    /// Update reservoir with additional samples
    pub fn update(&mut self, lights: &[Light], point: Vec3, normal: Vec3, rng: &mut Rng) {
        // Sample random lights
        for _ in 0..self.num_taps {
            let light_idx = rng.next_u32() % lights.len() as u32;
            let light = &lights[light_idx as usize];
            
            let light_sample = light.sample_point(rng);
            let direction = (light_sample - point).normalized();
            
            // Compute lighting weight
            let dist_sq = (light_sample - point).length_squared();
            let dist = dist_sq.sqrt();
            let cos_normal = normal.dot(direction).abs();
            let cos_light = normal.dot((point - light_sample).normalized()).abs();
            
            let weight = if dist_sq > 1e-6 && cos_normal > 1e-7 && cos_light > 1e-7 {
                light.intensity * cos_normal * cos_light / (dist * dist)
            } else {
                0.0
            };
            
            let pdf = 1.0 / (4.0 * std::f32::consts::PI * light.radius * light.radius).max(1e-7);
            
            self.reservoir.update(light_idx, light_sample, direction, weight, pdf);
        }
    }

    /// Get the selected light sample
    pub fn get_light_sample(&self) -> Option<&ReservoirEntry> {
        if self.reservoir.sample_count > 0 {
            Some(self.reservoir.sample())
        } else {
            None
        }
    }
}

/// Light sampling strategy
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LightSamplingStrategy {
    /// Uniform random light selection
    Uniform,
    /// Importance sampling based on light power
    Importance,
    /// ReSTIR reservoir-based sampling
    ReSTIR,
}

/// Sample a light using ReSTIR
pub fn sample_light_restir(
    lights: &[Light],
    point: Vec3,
    normal: Vec3,
    strategy: LightSamplingStrategy,
    rng: &mut Rng,
) -> (ReservoirEntry, f32) {
    match strategy {
        LightSamplingStrategy::Uniform => {
            // Uniform random light selection
            let light_idx = rng.next_u32() % lights.len() as u32;
            let light = &lights[light_idx as usize];
            let light_sample = light.sample_point(rng);
            let direction = (light_sample - point).normalized();
            
            let dist_sq = (light_sample - point).length_squared();
            let dist = dist_sq.sqrt();
            let cos_normal = normal.dot(direction).abs();
            let cos_light = normal.dot((point - light_sample).normalized()).abs();
            
            let weight = if dist_sq > 1e-6 && cos_normal > 1e-7 && cos_light > 1e-7 {
                light.intensity * cos_normal * cos_light / (dist * dist)
            } else {
                0.0
            };
            
            let pdf = 1.0 / lights.len() as f32;
            
            (ReservoirEntry {
                light_index: light_idx,
                light_sample,
                direction,
                weight,
                pdf,
            }, pdf)
        }
        
        LightSamplingStrategy::Importance => {
            // Importance sampling based on light power
            let total_power: f32 = lights.iter().map(|l| l.intensity).sum();
            let mut cumulative = 0.0;
            let mut light_idx = 0u32;
            
            for (i, light) in lights.iter().enumerate() {
                cumulative += light.intensity / total_power;
                if rng.next_f32() < cumulative {
                    light_idx = i as u32;
                    break;
                }
            }
            
            let light = &lights[light_idx as usize];
            let light_sample = light.sample_point(rng);
            let direction = (light_sample - point).normalized();
            
            let dist_sq = (light_sample - point).length_squared();
            let dist = dist_sq.sqrt();
            let cos_normal = normal.dot(direction).abs();
            let cos_light = normal.dot((point - light_sample).normalized()).abs();
            
            let weight = if dist_sq > 1e-6 && cos_normal > 1e-7 && cos_light > 1e-7 {
                light.intensity * cos_normal * cos_light / (dist * dist)
            } else {
                0.0
            };
            
            let pdf = light.intensity / total_power;
            
            (ReservoirEntry {
                light_index: light_idx,
                light_sample,
                direction,
                weight,
                pdf,
            }, pdf)
        }
        
        LightSamplingStrategy::ReSTIR => {
            // ReSTIR reservoir-based sampling
            let mut context = ReSTIRContext::new(5, true);
            context.update(lights, point, normal, rng);
            
            context.get_light_sample().copied().unwrap_or_default()
        }
    }
}

/// Evaluate lighting contribution from a reservoir sample
pub fn evaluate_lighting(
    reservoir: &ReservoirEntry,
    point: Vec3,
    normal: Vec3,
    albedo: Vec3,
) -> Vec3 {
    let direction = reservoir.direction;
    let dist_sq = (reservoir.light_sample - point).length_squared();
    let dist = dist_sq.sqrt();
    
    if dist_sq < 1e-6 {
        return Vec3::ZERO;
    }
    
    // Cosine weights
    let cos_normal = normal.dot(direction).abs();
    let cos_light = normal.dot((point - reservoir.light_sample).normalized()).abs();
    
    if cos_normal < 1e-7 || cos_light < 1e-7 {
        return Vec3::ZERO;
    }
    
    // Lighting calculation
    let light_factor = cos_normal * cos_light / (dist_sq);
    let irradiance = reservoir.weight * light_factor / reservoir.pdf.max(1e-7);
    
    albedo * irradiance
}

/// Spatio-temporal reuse for ReSTIR
pub fn spatiotemporal_reuse(
    current: &ReservoirEntry,
    previous: &ReservoirEntry,
    point: Vec3,
    normal: Vec3,
    temporal_weight: f32,
) -> ReservoirEntry {
    // Blend between current and previous frame's best sample
    let mut result = *current;
    
    if temporal_weight > 0.0 {
        // Compute weights for temporal blend
        let current_weight = current.weight;
        let previous_weight = previous.weight;
        
        // Stochastic decision to use previous sample
        let prob = temporal_weight * previous_weight / (current_weight + temporal_weight * previous_weight + 1e-7);
        
        if prob > 0.5 {
            result = *previous;
        }
    }
    
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_reservoir_init() {
        let mut rng = Rng::new(42);
        let light = Light {
            position: Vec3::new(0.0, 10.0, 0.0),
            color: Vec3::new(1.0, 1.0, 1.0),
            intensity: 100.0,
            radius: 2.0,
        };
        
        let mut reservoir = Reservoir::new();
        reservoir.update(0, light.position, Vec3::Z, 50.0, 0.1);
        
        assert_eq!(reservoir.sample_count, 1);
        assert!(reservoir.weight_sum > 0.0);
    }
    
    #[test]
    fn test_light_sampling_uniform() {
        let lights = vec![
            Light {
                position: Vec3::new(0.0, 10.0, 0.0),
                color: Vec3::new(1.0, 1.0, 1.0),
                intensity: 100.0,
                radius: 2.0,
            },
            Light {
                position: Vec3::new(5.0, 10.0, 0.0),
                color: Vec3::new(1.0, 0.5, 0.5),
                intensity: 50.0,
                radius: 1.0,
            },
        ];
        
        let mut rng = Rng::new(123);
        let point = Vec3::new(0.0, 0.0, 0.0);
        let normal = Vec3::Y;
        
        let (sample, pdf) = sample_light_restir(&lights, point, normal, LightSamplingStrategy::Uniform, &mut rng);
        
        assert!(sample.light_index < lights.len() as u32);
        assert!(pdf > 0.0);
    }
    
    #[test]
    fn test_light_sampling_importance() {
        let lights = vec![
            Light {
                position: Vec3::new(0.0, 10.0, 0.0),
                color: Vec3::new(1.0, 1.0, 1.0),
                intensity: 100.0,
                radius: 2.0,
            },
            Light {
                position: Vec3::new(5.0, 10.0, 0.0),
                color: Vec3::new(1.0, 0.5, 0.5),
                intensity: 50.0,
                radius: 1.0,
            },
        ];
        
        let mut rng = Rng::new(456);
        let point = Vec3::new(0.0, 0.0, 0.0);
        let normal = Vec3::Y;
        
        let (sample, pdf) = sample_light_restir(&lights, point, normal, LightSamplingStrategy::Importance, &mut rng);
        
        assert!(sample.light_index < lights.len() as u32);
        assert!(pdf > 0.0);
    }
    
    #[test]
    fn test_restir_reservoir() {
        let lights = vec![
            Light {
                position: Vec3::new(0.0, 10.0, 0.0),
                color: Vec3::new(1.0, 1.0, 1.0),
                intensity: 100.0,
                radius: 2.0,
            },
        ];
        
        let mut rng = Rng::new(789);
        let point = Vec3::new(0.0, 0.0, 0.0);
        let normal = Vec3::Y;
        
        let (sample, _pdf) = sample_light_restir(&lights, point, normal, LightSamplingStrategy::ReSTIR, &mut rng);
        
        assert_eq!(sample.light_index, 0);
    }
}
