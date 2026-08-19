//! Mesh component - vertex and index buffers
use litt_math::*;
use bytemuck::{Pod, Zeroable};

#[derive(Clone, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Vertex {
    pub position: Vec3,
    pub normal: Vec3,
    pub texcoord: Vec2,
}

#[derive(Clone, Debug, Default)]
pub struct Mesh {
    pub vertices: Vec<Vertex>,
    pub indices: Vec<u32>,
    pub bounding_box: Option<Bbox>,
}

impl Mesh {
    pub fn new() -> Self { Self::default() }
    pub fn from_triangles(tris: &[(Vec3, Vec3, Vec3)]) -> Self {
        let mut mesh = Self::new();
        for (i, (v0, v1, v2)) in tris.iter().enumerate() {
            let normal = v1.sub(*v0).cross(v2.sub(*v0)).normalized();
            mesh.vertices.extend_from_slice(&[
                Vertex { position: *v0, normal, texcoord: Vec2::new(0.0, 0.0) },
                Vertex { position: *v1, normal, texcoord: Vec2::new(1.0, 0.0) },
                Vertex { position: *v2, normal, texcoord: Vec2::new(0.5, 1.0) },
            ]);
            let base = (i * 3) as u32;
            mesh.indices.extend_from_slice(&[base, base+1, base+2]);
        }
        mesh.compute_bounds();
        mesh
    }
    pub fn compute_bounds(&mut self) {
        if self.vertices.is_empty() { return; }
        let mut min = self.vertices[0].position;
        let mut max = min;
        for v in &self.vertices {
            min = Vec3::new(min.0.min(v.position.0), min.1.min(v.position.1), min.2.min(v.position.2));
            max = Vec3::new(max.0.max(v.position.0), max.1.max(v.position.1), max.2.max(v.position.2));
        }
        self.bounding_box = Some(Bbox::new(min, max));
    }
}
