//! Model loading — GLTF and OBJ format support.
//! Outputs GPU-friendly mesh data (vertex buffers, index buffers, UVs, normals).

use litt_math::{Vec3, Mat4};
use super::handle::{AssetHandle, AssetState};

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
        // Simplified GLTF parser — real implementation would use a proper parser
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
            .map_err(|e| format!("Failed to read file '{}': {}", path, e))?;
        Self::load_from_bytes(&data)
    }
}

/// OBJ loader
pub struct ObjLoader;

impl ObjLoader {
    /// Load an OBJ model from bytes
    pub fn load_from_bytes(data: &[u8]) -> Result<Model, String> {
        let content = String::from_utf8_lossy(data);
        let mut model = Model::new("obj_model");

        let mut positions = Vec::new();
        let mut normals = Vec::new();
        let mut uvs = Vec::new();
        let mut vertices = Vec::new();
        let mut indices = Vec::new();

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
                "f" => {
                    for i in 1..parts.len() {
                        let vertex: Vec<usize> = parts[i]
                            .split('/')
                            .filter_map(|s| s.parse().ok())
                            .collect();
                        if vertex.len() >= 3 {
                            let vi = vertex[0] - 1;
                            let ni = if vertex.len() > 2 { vertex[2] - 1 } else { 0 };
                            let ti = if vertex.len() > 1 { vertex[1] - 1 } else { 0 };

                            let pos = if vi < positions.len() { positions[vi] } else { Vec3::ZERO };
                            let norm = if ni < normals.len() { normals[ni] } else { Vec3::Y };
                            let uv = if ti < uvs.len() { uvs[ti] } else { (0.0, 0.0) };

                            vertices.push(Vertex::new(pos, norm, uv));
                            indices.push(vertices.len() as u32 - 1);
                        }
                    }
                }
                _ => {}
            }
        }

        if !vertices.is_empty() {
            let mut mesh = Mesh::new("obj_mesh");
            mesh.vertices = vertices;
            mesh.indices = indices;
            mesh.compute_bounds();
            model.add_mesh(mesh);
        }

        Ok(model)
    }

    /// Load an OBJ model from file
    pub fn load_from_file(path: &str) -> Result<Model, String> {
        let data = std::fs::read(path)
            .map_err(|e| format!("Failed to read file '{}': {}", path, e))?;
        Self::load_from_bytes(&data)
    }
}
