//! World bridge -- converts a [`SceneGraph`] into a renderable path-tracer
//! [`Scene`] using the world's REAL assets.
//!
//! This is the native deployment path for AI-generated worlds: every node's
//! `model:<name>` tag is resolved against `<base>/models/<name>.obj` and its
//! triangles are transformed (yaw + uniform scale + translation) into the
//! scene. Nodes without a loadable model fall back to tag-colored marker
//! spheres so a world always renders something meaningful.
//!
//! Deterministic by construction: nodes are visited in ascending id order
//! and materials come from a fixed tag palette.

use std::collections::HashMap;

use litt_asset::ObjLoader;
use litt_math::Vec3;
use litt_pathtracer::{Light, MaterialEntry, Scene, Sphere, Triangle};
use litt_scene::SceneGraph;

/// Material slots used by the bridge (fixed palette, index-stable).
pub mod mat {
    pub const GROUND: u32 = 0;
    pub const STRUCTURE: u32 = 1;
    pub const ACCENT: u32 = 2;
    pub const DETAIL: u32 = 3;
    pub const PICKUP: u32 = 4;
    pub const ENEMY: u32 = 5;
    pub const GOAL: u32 = 6;
    pub const DEFAULT: u32 = 7;
}

/// Hard ceiling so a pathological world cannot explode GPU upload sizes.
const MAX_TRIANGLES: usize = 250_000;

/// Outcome summary for HUD / logs / AI transcripts.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct BridgeStats {
    pub nodes_rendered: usize,
    pub triangles: usize,
    pub spheres: usize,
    pub meshes_loaded: usize,
    /// Model names referenced but not found on disk.
    pub missing_models: Vec<String>,
}

/// Build the path-tracer scene for a world rooted at `base_dir`
/// (the folder containing `models/`).
pub fn build_render_scene(graph: &SceneGraph, base_dir: &str) -> (Scene, BridgeStats) {
    let mut scene = Scene::new();
    let mut stats = BridgeStats::default();
    let mut mesh_cache: HashMap<String, Option<litt_asset::Model>> = HashMap::new();
    let mut min = Vec3::new(f32::MAX, f32::MAX, f32::MAX);
    let mut max = Vec3::new(f32::MIN, f32::MIN, f32::MIN);

    let mut ids: Vec<u32> = graph.nodes.keys().copied().collect();
    ids.sort_unstable();

    // Fixed lighting: one warm sun high above the plaza plus soft fill.
    scene.add_material(ground_material());
    scene.add_material(structure_material());
    scene.add_material(accent_material());
    scene.add_material(detail_material());
    scene.add_material(pickup_material());
    scene.add_material(enemy_material());
    scene.add_material(goal_material());
    scene.add_material(default_material());
    scene.add_light(Light {
        position: Vec3::new(18.0, 30.0, 12.0),
        color: Vec3::new(1.0, 0.96, 0.88),
        intensity: 2.4,
        radius: 1.5,
    });
    scene.add_light(Light {
        position: Vec3::new(-20.0, 14.0, -16.0),
        color: Vec3::new(0.55, 0.62, 0.75),
        intensity: 0.8,
        radius: 3.0,
    });

    for id in ids {
        if id == graph.root_id {
            continue;
        }
        let Some(node) = graph.get(id) else { continue };
        if !node.visible {
            continue;
        }

        let yaw = quat_yaw(&node.rotation);
        let scale = node.scale.0.max(0.001);
        let pos = node.position;

        // Resolve model tag -> obj file -> transformed triangles.
        let model_tag = node.tags.iter().find(|t| t.starts_with("model:")).map(|t| t["model:".len()..].to_string());
        let mut rendered_mesh = false;
        if let Some(name) = &model_tag {
            let entry = mesh_cache.entry(name.clone()).or_insert_with(|| {
                let path = format!("{}/models/{}.obj", base_dir.trim_end_matches('/'), name);
                ObjLoader::load_from_file(&path).ok()
            });
            match entry {
                Some(model) => {
                    stats.meshes_loaded += 1;
                    'meshes: for mesh in &model.meshes {
                        let idx = &mesh.indices;
                        for tri in idx.chunks_exact(3) {
                            if scene.triangles.len() >= MAX_TRIANGLES || stats.triangles >= MAX_TRIANGLES {
                                break 'meshes;
                            }
                            let a = mesh.vertices[tri[0] as usize].position;
                            let b = mesh.vertices[tri[1] as usize].position;
                            let c = mesh.vertices[tri[2] as usize].position;
                            let ta = transform(a, pos, yaw, scale);
                            let tb = transform(b, pos, yaw, scale);
                            let tc = transform(c, pos, yaw, scale);
                            let normal = face_normal(ta, tb, tc);
                            let material_id = material_for_tags(&node.tags);
                            scene.add_triangle(Triangle { v0: ta, v1: tb, v2: tc, normal, material_id });
                            stats.triangles += 1;
                            grow_bounds(&ta, &mut min, &mut max);
                            grow_bounds(&tb, &mut min, &mut max);
                            grow_bounds(&tc, &mut min, &mut max);
                        }
                    }
                    rendered_mesh = true;
                }
                None => {
                    if !stats.missing_models.contains(name) {
                        stats.missing_models.push(name.clone());
                    }
                }
            }
        }

        // Fallback markers for nodes without (loadable) geometry.
        if !rendered_mesh {
            let (radius, material_id) = fallback_for_tags(&node.tags);
            let s = Sphere {
                center: pos,
                radius,
                material_id,
                _pad: [0.0; 3],
            };
            scene.add_sphere(s);
            stats.spheres += 1;
            grow_bounds(&pos, &mut min, &mut max);
        }
        stats.nodes_rendered += 1;
    }

    // Ground plane under everything -- always present so cameras have footing.
    {
        let has_content = min.0 <= max.0;
        let extent = if has_content {
            60.0f32.max((max.0 - min.0).abs()).max((max.2 - min.2).abs()) + 10.0
        } else {
            60.0
        };
        let gy = if has_content { min.1 - 0.05 } else { -0.05 };
        let c0 = Vec3::new(-extent, gy, -extent);
        let c1 = Vec3::new(extent, gy, -extent);
        let c2 = Vec3::new(extent, gy, extent);
        let c3 = Vec3::new(-extent, gy, extent);
        for (a, b, c) in [(c0, c1, c2), (c0, c2, c3)] {
            scene.add_triangle(Triangle {
                v0: a, v1: b, v2: c,
                normal: Vec3::Y,
                material_id: mat::GROUND,
            });
            stats.triangles += 1;
        }
    }

    if min.0 <= max.0 {
        scene.bounds = litt_pathtracer::SceneBounds { min, max };
    }
    (scene, stats)
}

fn transform(v: Vec3, pos: Vec3, yaw: f32, scale: f32) -> Vec3 {
    let (sy, cy) = yaw.sin_cos();
    let x = v.0 * scale;
    let z = v.2 * scale;
    Vec3::new(
        pos.0 + x * cy + z * sy,
        pos.1 + v.1 * scale,
        pos.2 - x * sy + z * cy,
    )
}

/// Yaw angle (radians) from a unit quaternion [x, y, z, w].
fn quat_yaw(q: &[f32; 4]) -> f32 {
    let (x, y, z, w) = (q[0], q[1], q[2], q[3]);
    (2.0 * (y * w + x * z)).atan2(1.0 - 2.0 * (y * y + z * z))
}

fn face_normal(a: Vec3, b: Vec3, c: Vec3) -> Vec3 {
    let e1 = Vec3::new(b.0 - a.0, b.1 - a.1, b.2 - a.2);
    let e2 = Vec3::new(c.0 - a.0, c.1 - a.1, c.2 - a.2);
    let n = Vec3::new(
        e1.1 * e2.2 - e1.2 * e2.1,
        e1.2 * e2.0 - e1.0 * e2.2,
        e1.0 * e2.1 - e1.1 * e2.0,
    );
    let len = (n.0 * n.0 + n.1 * n.1 + n.2 * n.2).sqrt();
    if len > 1e-8 {
        Vec3::new(n.0 / len, n.1 / len, n.2 / len)
    } else {
        Vec3::Y
    }
}

fn grow_bounds(p: &Vec3, min: &mut Vec3, max: &mut Vec3) {
    min.0 = min.0.min(p.0); min.1 = min.1.min(p.1); min.2 = min.2.min(p.2);
    max.0 = max.0.max(p.0); max.1 = max.1.max(p.1); max.2 = max.2.max(p.2);
}

fn material_for_tags(tags: &[String]) -> u32 {
    for t in tags {
        match t.as_str() {
            "pickup" | "coin" | "score" => return mat::PICKUP,
            "enemy" | "hazard" => return mat::ENEMY,
            "goal" | "win" => return mat::GOAL,
            "accent" => return mat::ACCENT,
            _ => {}
        }
    }
    mat::STRUCTURE
}

fn fallback_for_tags(tags: &[String]) -> (f32, u32) {
    for t in tags {
        match t.as_str() {
            "pickup" | "coin" | "score" => return (0.45, mat::PICKUP),
            "enemy" | "hazard" => return (0.85, mat::ENEMY),
            "goal" | "win" => return (1.1, mat::GOAL),
            _ => {}
        }
    }
    (0.6, mat::DETAIL)
}

// -- fixed palette -----------------------------------------------------------

fn ground_material() -> MaterialEntry {
    MaterialEntry { albedo: Vec3::new(0.38, 0.44, 0.34), roughness: 0.95, metallic: 0.0, ior: 1.45, emissive: Vec3::ZERO, light_intensity: 0.0 }
}

fn structure_material() -> MaterialEntry {
    MaterialEntry { albedo: Vec3::new(0.55, 0.52, 0.48), roughness: 0.7, metallic: 0.02, ior: 1.45, emissive: Vec3::ZERO, light_intensity: 0.0 }
}

fn accent_material() -> MaterialEntry {
    MaterialEntry { albedo: Vec3::new(0.85, 0.40, 0.20), roughness: 0.5, metallic: 0.05, ior: 1.45, emissive: Vec3::ZERO, light_intensity: 0.0 }
}

fn detail_material() -> MaterialEntry {
    MaterialEntry { albedo: Vec3::new(0.30, 0.32, 0.36), roughness: 0.8, metallic: 0.0, ior: 1.45, emissive: Vec3::ZERO, light_intensity: 0.0 }
}

fn pickup_material() -> MaterialEntry {
    MaterialEntry { albedo: Vec3::new(1.0, 0.82, 0.25), roughness: 0.15, metallic: 1.0, ior: 1.45, emissive: Vec3::ZERO, light_intensity: 0.0 }
}

fn enemy_material() -> MaterialEntry {
    MaterialEntry { albedo: Vec3::new(0.75, 0.12, 0.10), roughness: 0.6, metallic: 0.0, ior: 1.45, emissive: Vec3::new(0.08, 0.0, 0.0), light_intensity: 0.0 }
}

fn goal_material() -> MaterialEntry {
    MaterialEntry { albedo: Vec3::new(0.95, 0.65, 0.15), roughness: 0.3, metallic: 0.2, ior: 1.45, emissive: Vec3::new(0.55, 0.35, 0.06), light_intensity: 1.2 }
}

fn default_material() -> MaterialEntry {
    MaterialEntry { albedo: Vec3::new(0.7, 0.7, 0.72), roughness: 0.75, metallic: 0.0, ior: 1.45, emissive: Vec3::ZERO, light_intensity: 0.0 }
}

#[cfg(test)]
mod tests {
    use super::*;
    use litt_scene::SceneNode;

    fn graph_with(node_fn: impl FnOnce(&mut SceneNode)) -> SceneGraph {
        let mut g = SceneGraph::new();
        let id = g.create_node("Thing", Some(g.root_id));
        node_fn(g.get_mut(id).unwrap());
        g
    }

    #[test]
    fn empty_world_still_has_ground_and_lights() {
        let g = SceneGraph::new();
        let (scene, stats) = build_render_scene(&g, ".");
        assert_eq!(stats.nodes_rendered, 0);
        assert!(scene.triangles.len() >= 2); // ground pair
        assert_eq!(scene.lights.len(), 2);
        assert_eq!(scene.materials.len(), 8);
    }

    #[test]
    fn missing_model_falls_back_to_tagged_sphere() {
        let g = graph_with(|n| {
            n.position = Vec3::new(3.0, 0.0, 4.0);
            n.add_tag("pickup");
            n.add_tag("model:definitely_not_there");
        });
        let (scene, stats) = build_render_scene(&g, ".");
        assert_eq!(stats.missing_models, vec!["definitely_not_there".to_string()]);
        assert_eq!(stats.spheres, 1);
        // Copy fields by value: Sphere is #[repr(packed)], never reference its fields.
        let mid = scene.spheres[0].material_id;
        let ctr = scene.spheres[0].center;
        assert_eq!(mid, mat::PICKUP);
        assert_eq!(ctr, Vec3::new(3.0, 0.0, 4.0));
    }

    #[test]
    fn real_model_is_loaded_transformed_and_counted() {
        // Write a tiny valid OBJ into a temp world root.
        let dir = std::env::temp_dir().join("litt_bridge_test");
        std::fs::create_dir_all(dir.join("models")).unwrap();
        std::fs::write(
            dir.join("models/crate.obj"),
            "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\nf 1 2 3\nf 2 4 3\n",
        )
        .unwrap();

        let g = graph_with(|n| {
            n.position = Vec3::new(10.0, 0.0, -5.0);
            n.scale = Vec3::new(2.0, 2.0, 2.0);
            n.add_tag("model:crate");
        });
        let base = dir.to_str().unwrap();
        let (scene, stats) = build_render_scene(&g, base);

        assert_eq!(stats.meshes_loaded, 1);
        // 2 mesh triangles + 2 ground-plane triangles.
        assert_eq!(stats.triangles, 4);
        assert_eq!(scene.triangles.len(), 4);
        // Vertex (1,0,0) scaled x2 and translated -> must exist exactly at (12,-5)-ish plane.
        let found = scene.triangles.iter().any(|t| {
            let t = *t; // copy out of packed struct
            (t.v0.0 - 12.0).abs() < 1e-4 && (t.v0.2 - (-5.0)).abs() < 1e-4
                || (t.v1.0 - 12.0).abs() < 1e-4 && (t.v1.2 - (-5.0)).abs() < 1e-4
                || (t.v2.0 - 12.0).abs() < 1e-4 && (t.v2.2 - (-5.0)).abs() < 1e-4
        });
        assert!(found, "transformed vertex not found in output triangles");
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn yaw_rotation_moves_vertices() {
        let dir = std::env::temp_dir().join("litt_bridge_yaw");
        std::fs::create_dir_all(dir.join("models")).unwrap();
        std::fs::write(dir.join("models/strip.obj"), "v 1 0 0\nv 2 0 0\nv 1 1 0\nf 1 2 3\n").unwrap();

        let g = graph_with(|n| {
            n.position = Vec3::ZERO;
            n.rotation = [0.0, std::f32::consts::FRAC_1_SQRT_2, 0.0, std::f32::consts::FRAC_1_SQRT_2]; // +90 deg yaw
            n.add_tag("model:strip");
        });
        let (scene, _) = build_render_scene(&g, dir.to_str().unwrap());
        // Local +X should rotate toward -Z (yaw CCW): some vertex near (0,0,-1..-2).
        let rotated = scene.triangles.iter().any(|t| {
            let t = *t; // copy out of packed struct
            t.v0.0.abs() < 1e-3 && t.v0.2 < -0.9 && t.v0.2 > -2.1
                || t.v1.0.abs() < 1e-3 && t.v1.2 < -0.9 && t.v1.2 > -2.1
                || t.v2.0.abs() < 1e-3 && t.v2.2 < -0.9 && t.v2.2 > -2.1
        });
        assert!(rotated, "expected +X axis to map onto -Z after 90deg yaw");
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn invisible_nodes_are_skipped() {
        let g = graph_with(|n| {
            n.visible = false;
            n.position = Vec3::new(1.0, 1.0, 1.0);
            n.add_tag("enemy");
        });
        let (_, stats) = build_render_scene(&g, ".");
        assert_eq!(stats.nodes_rendered, 0);
        assert_eq!(stats.spheres, 0);
    }
}
