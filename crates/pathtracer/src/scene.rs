//! Scene data structures for the path tracer.
//! Simple struct-based scene -- no ECS, no asset pipeline.

use litt_math::*;
use bytemuck::{Pod, Zeroable};

/// A triangle in the scene
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C, packed)]
pub struct Triangle {
    pub v0: Vec3,
    pub v1: Vec3,
    pub v2: Vec3,
    pub normal: Vec3,
    pub material_id: u32,
}

/// A sphere in the scene
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C, packed)]
pub struct Sphere {
    pub center: Vec3,
    pub radius: f32,
    pub material_id: u32,
    pub _pad: [f32; 3],
}

/// Light source
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C, packed)]
pub struct Light {
    pub position: Vec3,
    pub color: Vec3,
    pub intensity: f32,
    pub radius: f32,
}

/// Scene bounding box
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct SceneBounds {
    pub min: Vec3,
    pub max: Vec3,
}

/// Material index
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct MaterialEntry {
    pub albedo: Vec3,
    pub roughness: f32,
    pub metallic: f32,
    pub ior: f32,
    pub emissive: Vec3,
    pub light_intensity: f32,
}

/// The complete scene
#[derive(Debug)]
pub struct Scene {
    pub triangles: Vec<Triangle>,
    pub spheres: Vec<Sphere>,
    pub lights: Vec<Light>,
    pub materials: Vec<MaterialEntry>,
    pub bounds: SceneBounds,
}

impl Scene {
    pub fn new() -> Self {
        Self {
            triangles: Vec::new(),
            spheres: Vec::new(),
            lights: Vec::new(),
            materials: Vec::new(),
            bounds: SceneBounds {
                min: Vec3::new(-100.0, -100.0, -100.0),
                max: Vec3::new(100.0, 100.0, 100.0),
            },
        }
    }

    /// Build a default test scene
    pub fn default_test_scene() -> Self {
        let mut scene = Self::new();

        // Floor
        scene.add_triangle(Triangle {
            v0: Vec3::new(-10.0, 0.0, -10.0),
            v1: Vec3::new(10.0, 0.0, -10.0),
            v2: Vec3::new(0.0, 0.0, 10.0),
            normal: Vec3::Y,
            material_id: 0,
        });
        scene.add_triangle(Triangle {
            v0: Vec3::new(-10.0, 0.0, -10.0),
            v1: Vec3::new(0.0, 0.0, 10.0),
            v2: Vec3::new(10.0, 0.0, 10.0),
            normal: Vec3::Y,
            material_id: 0,
        });

        // Back wall
        scene.add_triangle(Triangle {
            v0: Vec3::new(-10.0, 0.0, -10.0),
            v1: Vec3::new(-10.0, 10.0, 0.0),
            v2: Vec3::new(10.0, 0.0, -10.0),
            normal: Vec3::new(0.0, 0.0, -1.0),
            material_id: 1,
        });
        scene.add_triangle(Triangle {
            v0: Vec3::new(-10.0, 0.0, -10.0),
            v1: Vec3::new(10.0, 0.0, -10.0),
            v2: Vec3::new(10.0, 10.0, 0.0),
            normal: Vec3::new(0.0, 0.0, -1.0),
            material_id: 1,
        });

        // Left wall
        scene.add_triangle(Triangle {
            v0: Vec3::new(-10.0, 0.0, -10.0),
            v1: Vec3::new(-10.0, 10.0, 0.0),
            v2: Vec3::new(-10.0, 0.0, 10.0),
            normal: Vec3::new(1.0, 0.0, 0.0),
            material_id: 2,
        });
        scene.add_triangle(Triangle {
            v0: Vec3::new(-10.0, 0.0, -10.0),
            v1: Vec3::new(-10.0, 0.0, 10.0),
            v2: Vec3::new(-10.0, 10.0, 0.0),
            normal: Vec3::new(1.0, 0.0, 0.0),
            material_id: 2,
        });

        // Right wall
        scene.add_triangle(Triangle {
            v0: Vec3::new(10.0, 0.0, -10.0),
            v1: Vec3::new(10.0, 10.0, 0.0),
            v2: Vec3::new(10.0, 0.0, 10.0),
            normal: Vec3::new(-1.0, 0.0, 0.0),
            material_id: 3,
        });
        scene.add_triangle(Triangle {
            v0: Vec3::new(10.0, 0.0, -10.0),
            v1: Vec3::new(10.0, 0.0, 10.0),
            v2: Vec3::new(10.0, 10.0, 0.0),
            normal: Vec3::new(-1.0, 0.0, 0.0),
            material_id: 3,
        });

        // Ceiling
        scene.add_triangle(Triangle {
            v0: Vec3::new(-10.0, 10.0, -10.0),
            v1: Vec3::new(-10.0, 10.0, 10.0),
            v2: Vec3::new(10.0, 10.0, 0.0),
            normal: Vec3::new(0.0, -1.0, 0.0),
            material_id: 4,
        });
        scene.add_triangle(Triangle {
            v0: Vec3::new(-10.0, 10.0, -10.0),
            v1: Vec3::new(10.0, 10.0, 0.0),
            v2: Vec3::new(10.0, 10.0, -10.0),
            normal: Vec3::new(0.0, -1.0, 0.0),
            material_id: 4,
        });

        // Spheres
        scene.add_sphere(Sphere {
            center: Vec3::new(-2.0, 1.0, -4.0),
            radius: 1.0,
            material_id: 5,
            _pad: [0.0; 3],
        });
        scene.add_sphere(Sphere {
            center: Vec3::new(2.0, 1.0, -2.0),
            radius: 1.0,
            material_id: 6,
            _pad: [0.0; 3],
        });

        // Lights
        scene.add_light(Light {
            position: Vec3::new(0.0, 8.0, -5.0),
            color: Vec3::new(1.0, 0.95, 0.9),
            intensity: 50.0,
            radius: 2.0,
        });

        // Materials
        scene.add_material(MaterialEntry {
            albedo: Vec3::new(0.8, 0.8, 0.8),
            roughness: 0.8,
            metallic: 0.0,
            ior: 1.5,
            emissive: Vec3::ZERO,
            light_intensity: 0.0,
        }); // 0: floor
        scene.add_material(MaterialEntry {
            albedo: Vec3::new(0.2, 0.2, 0.2),
            roughness: 0.1,
            metallic: 0.0,
            ior: 1.5,
            emissive: Vec3::ZERO,
            light_intensity: 0.0,
        }); // 1: back wall
        scene.add_material(MaterialEntry {
            albedo: Vec3::new(0.8, 0.2, 0.2),
            roughness: 0.5,
            metallic: 0.0,
            ior: 1.5,
            emissive: Vec3::ZERO,
            light_intensity: 0.0,
        }); // 2: left wall (red)
        scene.add_material(MaterialEntry {
            albedo: Vec3::new(0.2, 0.2, 0.8),
            roughness: 0.5,
            metallic: 0.0,
            ior: 1.5,
            emissive: Vec3::ZERO,
            light_intensity: 0.0,
        }); // 3: right wall (blue)
        scene.add_material(MaterialEntry {
            albedo: Vec3::new(0.9, 0.9, 0.9),
            roughness: 0.9,
            metallic: 0.0,
            ior: 1.5,
            emissive: Vec3::ZERO,
            light_intensity: 0.0,
        }); // 4: ceiling
        scene.add_material(MaterialEntry {
            albedo: Vec3::new(0.8, 0.6, 0.4),
            roughness: 0.2,
            metallic: 0.0,
            ior: 1.5,
            emissive: Vec3::ZERO,
            light_intensity: 0.0,
        }); // 5: sphere (warm)
        scene.add_material(MaterialEntry {
            albedo: Vec3::new(0.2, 0.4, 0.8),
            roughness: 0.05,
            metallic: 1.0,
            ior: 2.0,
            emissive: Vec3::ZERO,
            light_intensity: 0.0,
        }); // 6: sphere (metal)

        // Update bounds
        scene.update_bounds();
        scene
    }

    pub fn add_triangle(&mut self, t: Triangle) {
        self.triangles.push(t);
    }

    pub fn add_sphere(&mut self, s: Sphere) {
        self.spheres.push(s);
    }

    pub fn add_light(&mut self, l: Light) {
        self.lights.push(l);
    }

    pub fn add_material(&mut self, m: MaterialEntry) {
        self.materials.push(m);
    }

    pub fn update_bounds(&mut self) {
        let mut min = Vec3::new(f32::INFINITY, f32::INFINITY, f32::INFINITY);
        let mut max = Vec3::new(-f32::INFINITY, -f32::INFINITY, -f32::INFINITY);

        for t in &self.triangles {
            for v in [t.v0, t.v1, t.v2] {
                min = Vec3::new(
                    f32::min(min.0, v.0),
                    f32::min(min.1, v.1),
                    f32::min(min.2, v.2),
                );
                max = Vec3::new(
                    f32::max(max.0, v.0),
                    f32::max(max.1, v.1),
                    f32::max(max.2, v.2),
                );
            }
        }
        for s in &self.spheres {
            min = Vec3::new(
                f32::min(min.0, s.center.0 - s.radius),
                f32::min(min.1, s.center.1 - s.radius),
                f32::min(min.2, s.center.2 - s.radius),
            );
            max = Vec3::new(
                f32::max(max.0, s.center.0 + s.radius),
                f32::max(max.1, s.center.1 + s.radius),
                f32::max(max.2, s.center.2 + s.radius),
            );
        }

        self.bounds = SceneBounds { min, max };
    }
}

/// Simple yaw/pitch camera used by the path tracer and fly controls.
#[derive(Clone, Copy, Debug)]
pub struct Camera {
    pub position: Vec3,
    /// (yaw, pitch) in radians
    pub rotation: Vec2,
    /// Vertical field of view in degrees
    pub fov: f32,
    pub aspect: f32,
}

impl Default for Camera {
    fn default() -> Self {
        Self {
            position: Vec3::new(0.0, 2.0, 8.0),
            rotation: Vec2::new(0.0, 0.0),
            fov: 90.0,
            aspect: 16.0 / 9.0,
        }
    }
}

impl Light {
    /// Sample a random point on the light surface (sphere approximation).
    pub fn sample_point(&self, rng: &mut crate::rng::Rng) -> Vec3 {
        // Uniform point on sphere around the light center
        let u = rng.next_f32();
        let v = rng.next_f32();
        let theta = 2.0 * core::f32::consts::PI * u;
        let z = 1.0 - 2.0 * v;
        let r = (1.0 - z * z).max(0.0).sqrt() * self.radius.max(0.001);
        Vec3::new(r * theta.cos(), r * theta.sin(), z) + self.position
    }
}
