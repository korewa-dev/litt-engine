//! Random number generation for path tracing.
//! GPU-compatible PCG random number generator.

use litt_math::*;

#[derive(Clone, Copy)]
pub struct Rng {
    state: u64,
}

impl Rng {
    pub fn new(seed: u64) -> Self {
        Self { state: seed }
    }

    pub fn next_u32(&mut self) -> u32 {
        let prev = self.state;
        self.state = prev.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        ((prev >> 22) ^ prev) as u32
    }

    pub fn next_f32(&mut self) -> f32 {
        (self.next_u32() >> 8) as f32 * (1.0 / 8388608.0)
    }

    pub fn next_vec3(&mut self) -> Vec3 {
        Vec3(self.next_f32(), self.next_f32(), self.next_f32())
    }

    pub fn random_disk(&mut self) -> Vec3 {
        let angle = self.next_f32() * 2.0 * core::f32::consts::PI;
        let r = self.next_f32().sqrt();
        Vec3(r * angle.cos(), r * angle.sin(), 0.0)
    }

    pub fn random_hemisphere(&mut self, normal: Vec3) -> Vec3 {
        let sign = if normal.1 >= 0.0 { 1.0 } else { -1.0 };
        let a = 1.0 / (1.0 + sign * normal.1);
        let tangent = Vec3(sign * normal.2 * a, -sign * normal.0 * a, -sign * a);
        let bitangent = normal.cross(tangent);
        let u = self.next_f32();
        let v = self.next_f32();
        let theta = 2.0 * core::f32::consts::PI * v;
        let r = u.sqrt();
        let phi = theta;
        let sp = r * phi.sin();
        let up = r * phi.cos();
        tangent * sp * phi.sin() + bitangent * sp * phi.cos() + normal * up
    }

    /// Russian roulette: return true if we should continue tracing
    pub fn russian_roulette(&mut self, throughput: Vec3) -> bool {
        let p = (throughput.0.max(throughput.1) + throughput.2) / 3.0;
        let q = p.min(0.95);
        self.next_f32() < q
    }
}
