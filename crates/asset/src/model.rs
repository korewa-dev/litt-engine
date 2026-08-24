//! Model loading -- GLTF and OBJ format support.
//! Outputs GPU-friendly mesh data (vertex buffers, index buffers, UVs, normals).

use litt_math::{Vec3, Mat4};
use super::handle::{AssetHandle, AssetType};
use std::collections::HashMap;

/// Vertex format for GPU mesh
#[derive(Clone, Debug)]
pub struct Vertex {
    pub position: Vec3,
    pub normal: Vec3,
    pub uv: (f32, f32),
    pub tangent: Vec3,
    pub color: [f32; 4],
    pub bone_indices: [u8; 4],
    pub bone_weights: [f32; 4],
}

impl Vertex {
    pub fn new(position: Vec3, normal: Vec3, uv: (f32, f32)) -> Self {
        Self {
            position,
            normal,
            uv,
            tangent: Vec3::X,
            color: [1.0, 1.0, 1.0, 1.0],
            bone_indices: [0, 0, 0, 0],
            bone_weights: [0.0, 0.0, 0.0, 0.0],
        }
    }
}

/// A loaded mesh
#[derive(Debug)]
pub struct Mesh {
    pub vertices: Vec<Vertex>,
    pub indices: Vec<u32>,
    pub name: String,
    pub bounding_box: (Vec3, Vec3),
}

impl Mesh {
    /// Create an empty mesh
    pub fn new(name: &str) -> Self {
        Self {
            vertices: Vec::new(),
            indices: Vec::new(),
            name: name.to_string(),
            bounding_box: (Vec3::ZERO, Vec3::ZERO),
        }
    }

    /// Compute bounding box from vertices
    pub fn compute_bounds(&mut self) {
        if self.vertices.is_empty() {
            return;
        }
        let mut min = self.vertices[0].position;
        let mut max = self.vertices[0].position;
        for v in &self.vertices {
            min = Vec3::new(
                min.0.min(v.position.0),
                min.1.min(v.position.1),
                min.2.min(v.position.2),
            );
            max = Vec3::new(
                max.0.max(v.position.0),
                max.1.max(v.position.1),
                max.2.max(v.position.2),
            );
        }
        self.bounding_box = (min, max);
    }

    /// Get vertex count
    pub fn vertex_count(&self) -> usize { self.vertices.len() }

    /// Get index count
    pub fn index_count(&self) -> usize { self.indices.len() }
}

/// A loaded model with multiple meshes
#[derive(Debug)]
pub struct Model {
    pub name: String,
    pub meshes: Vec<Mesh>,
    pub animations: Vec<Animation>,
    pub materials: Vec<MaterialRef>,
    pub transforms: Vec<Mat4>,
}

impl Model {
    /// Create an empty model
    pub fn new(name: &str) -> Self {
        Self {
            name: name.to_string(),
            meshes: Vec::new(),
            animations: Vec::new(),
            materials: Vec::new(),
            transforms: Vec::new(),
        }
    }

    /// Add a mesh to the model
    pub fn add_mesh(&mut self, mesh: Mesh) {
        self.meshes.push(mesh);
    }

    /// Get total vertex count
    pub fn total_vertices(&self) -> usize {
        self.meshes.iter().map(|m| m.vertex_count()).sum()
    }

    /// Get total index count
    pub fn total_indices(&self) -> usize {
        self.meshes.iter().map(|m| m.index_count()).sum()
    }
}

/// Reference to a material
#[derive(Debug, Clone)]
pub struct MaterialRef {
    pub handle: AssetHandle,
    pub name: String,
}

/// Keyframe animation data
#[derive(Debug, Clone)]
pub struct Keyframe {
    pub time: f32,
    pub position: Vec3,
    pub rotation: [f32; 4], // quaternion
    pub scale: Vec3,
}

/// An animation channel
#[derive(Debug, Clone)]
pub struct AnimationChannel {
    pub target_name: String,
    pub keyframes: Vec<Keyframe>,
}

/// A complete animation
#[derive(Debug, Clone)]
pub struct Animation {
    pub name: String,
    pub duration: f32,
    pub channels: Vec<AnimationChannel>,
}

/// GLTF loader
pub struct GltfLoader;

impl GltfLoader {
    /// Load a GLTF model from bytes
    pub fn load_from_bytes(data: &[u8]) -> Result<Model, String> {
        // Simplified GLTF parser -- real implementation would use a proper parser
        // This creates a default cube model as placeholder
        let mut model = Model::new("gltf_model");

        // Create a simple cube mesh
        let mut mesh = Mesh::new("cube");
        let size = 1.0;
        let vertices = vec![
            // Front face
            Vertex::new(Vec3::new(-size, -size, size), Vec3::Z, (0.0, 0.0)),
            Vertex::new(Vec3::new(size, -size, size), Vec3::Z, (1.0, 0.0)),
            Vertex::new(Vec3::new(size, size, size), Vec3::Z, (1.0, 1.0)),
            Vertex::new(Vec3::new(-size, size, size), Vec3::Z, (0.0, 1.0)),
            // Back face
            Vertex::new(Vec3::new(-size, -size, -size), -Vec3::Z, (0.0, 0.0)),
            Vertex::new(Vec3::new(-size, size, -size), -Vec3::Z, (1.0, 0.0)),
            Vertex::new(Vec3::new(size, size, -size), -Vec3::Z, (1.0, 1.0)),
            Vertex::new(Vec3::new(size, -size, -size), -Vec3::Z, (0.0, 1.0)),
            // Top face
            Vertex::new(Vec3::new(-size, size, -size), Vec3::Y, (0.0, 0.0)),
            Vertex::new(Vec3::new(-size, size, size), Vec3::Y, (1.0, 0.0)),
            Vertex::new(Vec3::new(size, size, size), Vec3::Y, (1.0, 1.0)),
            Vertex::new(Vec3::new(size, size, -size), Vec3::Y, (0.0, 1.0)),
            // Bottom face
            Vertex::new(Vec3::new(-size, -size, -size), -Vec3::Y, (0.0, 0.0)),
            Vertex::new(Vec3::new(size, -size, -size), -Vec3::Y, (1.0, 0.0)),
            Vertex::new(Vec3::new(size, -size, size), -Vec3::Y, (1.0, 1.0)),
            Vertex::new(Vec3::new(-size, -size, size), -Vec3::Y, (0.0, 1.0)),
        ];
        let indices = vec![
            0, 1, 2, 0, 2, 3,  // front
            4, 5, 6, 4, 6, 7,  // back
            8, 9, 10, 8, 10, 11, // top
            12, 13, 14, 12, 14, 15, // bottom
            3, 2, 6, 3, 6, 7,  // right
            0, 3, 7, 0, 7, 4,  // left
        ];

        mesh.vertices = vertices;
        mesh.indices = indices;
        mesh.compute_bounds();
        model.add_mesh(mesh);

        Ok(model)
    }

    /// Load a GLTF model from file
    pub fn load_from_file(path: &str) -> Result<Model, String> {
        let data = std::fs::read(path)
            .map_err(|e| format!("Failed to read file '{path}': {e}"))?;
        Self::load_from_bytes(&data)
    }
}

/// OBJ loader
pub struct ObjLoader;

impl ObjLoader {
    /// Load an OBJ model from bytes
    ///
    /// Group-aware: every `g <name>` (or `usemtl` switch) starts a new named
    /// mesh so part-based rigs survive loading. Faces may span global vertex
    /// indices; each mesh keeps its own local index buffer.
    pub fn load_from_bytes(data: &[u8]) -> Result<Model, String> {
        let content = String::from_utf8_lossy(data);
        let mut model = Model::new("obj_model");

        let mut positions = Vec::new();
        let mut normals = Vec::new();
        let mut uvs = Vec::new();

        // current group being assembled
        let mut cur_name = String::from("obj_mesh");
        let mut cur_mat = String::new();
        let mut vertices: Vec<Vertex> = Vec::new();
        let mut indices: Vec<u32> = Vec::new();
        // global "v//vn" pair -> local vertex index within the current group
        let mut remap: HashMap<(usize, usize), u32> = HashMap::new();

        macro_rules! flush_group {
            () => {
                if !vertices.is_empty() {
                    let mut mesh = Mesh::new(&cur_name.clone());
                    mesh.vertices = std::mem::take(&mut vertices);
                    mesh.indices = std::mem::take(&mut indices);
                    if !cur_mat.is_empty() {
                        model.materials.push(MaterialRef {
                            handle: AssetHandle::from_path(&cur_mat, AssetType::Material),
                            name: cur_mat.clone(),
                        });
                    }
                    mesh.compute_bounds();
                    model.add_mesh(mesh);
                    remap.clear();
                }
            };
        }

        for line in content.lines() {
            let line = line.trim();
            if line.is_empty() || line.starts_with('#') { continue; }

            let parts: Vec<&str> = line.split_whitespace().collect();
            if parts.is_empty() { continue; }

            match parts[0] {
                "v" => {
                    if parts.len() >= 4 {
                        let x: f32 = parts[1].parse().unwrap_or(0.0);
                        let y: f32 = parts[2].parse().unwrap_or(0.0);
                        let z: f32 = parts[3].parse().unwrap_or(0.0);
                        positions.push(Vec3::new(x, y, z));
                    }
                }
                "vn" => {
                    if parts.len() >= 4 {
                        let x: f32 = parts[1].parse().unwrap_or(0.0);
                        let y: f32 = parts[2].parse().unwrap_or(0.0);
                        let z: f32 = parts[3].parse().unwrap_or(0.0);
                        normals.push(Vec3::new(x, y, z));
                    }
                }
                "vt" => {
                    if parts.len() >= 3 {
                        let u: f32 = parts[1].parse().unwrap_or(0.0);
                        let v: f32 = parts[2].parse().unwrap_or(0.0);
                        uvs.push((u, v));
                    }
                }
                "g" | "o" => {
                    // named part boundary - flush and start a fresh mesh
                    flush_group!();
                    if parts.len() >= 2 {
                        cur_name = parts[1..].join("_");
                    }
                }
                "usemtl" => {
                    if parts.len() >= 2 && parts[1] != cur_mat {
                        flush_group!();
                        cur_mat = parts[1].to_string();
                        if cur_name == "obj_mesh" {
                            cur_name = cur_mat.clone();
                        }
                    }
                }
                "f" => {
                    // OBJ corner formats: "v", "v/vt", "v//vn", "v/vt/vn".
                    // Empty slots are legal ("1//3") -- keep them positional.
                    let mut corner_indices: Vec<u32> = Vec::new();
                    for corner in &parts[1..] {
                        let slots: Vec<Option<usize>> =
                            corner.split('/').map(|s| s.parse().ok()).collect();
                        let vi = match slots.first().and_then(|o| *o) {
                            Some(v) if v >= 1 => v - 1,
                            _ => continue,
                        };
                        let ti = slots.get(1).and_then(|o| *o).and_then(|v| v.checked_sub(1));
                        let ni = slots.get(2).and_then(|o| *o).and_then(|v| v.checked_sub(1));

                        let pos = if vi < positions.len() { positions[vi] } else { Vec3::ZERO };
                        let norm = match ni {
                            Some(n) if n < normals.len() => normals[n],
                            _ => Vec3::Y,
                        };
                        let uv = match ti {
                            Some(t) if t < uvs.len() => uvs[t],
                            _ => (0.0, 0.0),
                        };

                        let key = (vi, ni.unwrap_or(usize::MAX));
                        let local = *remap.entry(key).or_insert_with(|| {
                            vertices.push(Vertex::new(pos, norm, uv));
                            (vertices.len() - 1) as u32
                        });
                        corner_indices.push(local);
                    }
                    // Triangle-fan the polygon (handles tris, quads, ngons).
                    if corner_indices.len() >= 3 {
                        for i in 2..corner_indices.len() {
                            indices.push(corner_indices[0]);
                            indices.push(corner_indices[i - 1]);
                            indices.push(corner_indices[i]);
                        }
                    }
                }
                _ => {}
            }
        }

        flush_group!();

        if model.meshes.is_empty() {
            return Err("OBJ contained no faces".to_string());
        }

        Ok(model)
    }

    /// Load an OBJ model from file
    pub fn load_from_file(path: &str) -> Result<Model, String> {
        let data = std::fs::read(path)
            .map_err(|e| format!("Failed to read file '{path}': {e}"))?;
        Self::load_from_bytes(&data)
    }
}

#[cfg(test)]
mod obj_tests {
    use super::*;

    #[test]
    fn obj_loader_accepts_all_face_formats() {
        // Covers "v//vn" (generator output), bare "v", and "v/vt" corners.
        let obj = b"# tiny world\n\
            v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 0 1\n\
            vn 0 0 1\nvt 0 0\n\
            f 1//1 2//1 3//1\n\
            f 1 2 4\n\
            f 1/1 2/1 4/1\n";
        let model = ObjLoader::load_from_bytes(obj).unwrap();
        assert_eq!(model.meshes.len(), 1);
        // Every face must survive the parser (regression: v//vn was dropped).
        assert_eq!(model.meshes[0].indices.len(), 9);
    }

    #[test]
    fn obj_loader_fan_triangulates_quads() {
        let quad = b"v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n";
        let model = ObjLoader::load_from_bytes(quad).unwrap();
        assert_eq!(model.meshes[0].indices.len(), 6); // quad -> 2 triangles
    }

    #[test]
    fn obj_loader_skips_malformed_corners_gracefully() {
        let bad = b"v 0 0 0\nv 1 0 0\nv 1 1 0\nf 1 x 3\nf 1 2 3\n";
        let model = ObjLoader::load_from_bytes(bad).unwrap();
        // Malformed corner skipped, valid face still loads.
        assert_eq!(model.meshes[0].indices.len(), 3);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn obj_groups_become_named_meshes() {
        let obj = b"# part rig\n\
                    g knight_torso\nusemtl prop_metal\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1//1 2//1 3//1\n\
                    g knight_leg_l\nusemtl prop_metal_dk\nv 5 0 0\nv 6 0 0\nv 5 1 0\nf 4//2 5//2 6//2\n";
        let model = ObjLoader::load_from_bytes(obj).unwrap();
        assert_eq!(model.meshes.len(), 2, "each g-group must be its own mesh");
        assert_eq!(model.meshes[0].name, "knight_torso");
        assert_eq!(model.meshes[1].name, "knight_leg_l");
        // global vertex indices must be remapped per group
        assert_eq!(model.meshes[1].vertices[0].position.0, 5.0);
        assert_eq!(
            model.meshes[1].indices,
            vec![0, 1, 2],
            "second group indexes locally from zero"
        );
        assert_eq!(model.materials.len(), 2);
    }

    #[test]
    fn usemtl_switch_splits_when_no_groups() {
        let obj = b"v 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl red\nf 1 2 3\n";
        let model = ObjLoader::load_from_bytes(obj).unwrap();
        assert!(!model.meshes.is_empty());
        assert_eq!(model.meshes[0].name, "red");
    }
}
