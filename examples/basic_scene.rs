//! Basic example scene
use litt_math::*;
use litt_pathtracer::scene::*;

fn create_example_scene() -> Scene {
    let mut scene = Scene::default_test_scene();
    scene.add_sphere(Sphere {
        center: Vec3::new(3.0, 0.5, -3.0), radius: 0.5,
        material_id: 7, _pad: [0.0; 3],
    });
    scene.add_material(MaterialEntry {
        albedo: Vec3::new(0.9, 0.7, 0.3), roughness: 0.3, metallic: 0.0,
        ior: 1.5, emissive: Vec3::ZERO, light_intensity: 0.0,
    });
    scene.update_bounds();
    scene
}

fn main() {
    let scene = create_example_scene();
    eprintln!("Scene: {} triangles, {} spheres, {} lights",
        scene.triangles.len(), scene.spheres.len(), scene.lights.len());
}
