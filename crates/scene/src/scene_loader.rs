//! Scene loader -- loads scenes from files (GLTF, custom format).

use super::scene_graph::SceneGraph;
use super::scene_node::SceneNode;

/// Scene loader
pub struct SceneLoader;

impl SceneLoader {
    /// Load a scene from a GLTF file
    pub fn load_gltf(path: &str) -> Result<SceneGraph, String> {
        // Parse GLTF and create scene graph
        // This is a simplified loader -- real implementation would parse GLTF properly
        let data = std::fs::read(path)
            .map_err(|e| format!("Failed to read '{}': {}", path, e))?;

        let mut scene = SceneGraph::new();

        // Create root node
        let root_id = scene.create_node("Scene", None);

        // Create default nodes
        let camera_id = scene.create_node("Camera", Some(root_id));
        let light_id = scene.create_node("Light", Some(root_id));

        // Update camera and light with defaults
        if let Some(camera) = scene.nodes.get_mut(&camera_id) {
            camera.position = litt_math::Vec3::new(0.0, 2.0, 5.0);
            camera.tags.push("Camera".to_string());
        }
        if let Some(light) = scene.nodes.get_mut(&light_id) {
            light.position = litt_math::Vec3::new(0.0, 8.0, -5.0);
            light.tags.push("Light".to_string());
        }

        Ok(scene)
    }

    /// Load a scene from a custom binary format
    pub fn load_binary(path: &str) -> Result<SceneGraph, String> {
        let data = std::fs::read(path)
            .map_err(|e| format!("Failed to read '{}': {}", path, e))?;

        let mut scene = SceneGraph::new();

        // Parse simple binary format:
        // [num_nodes: u32] [node_data * num_nodes]
        // node_data: [name_len: u16] [name: bytes] [position: f32x3] [rotation: f32x4] [scale: f32x3] [parent_id: u32]
        let mut offset = 0;
        if offset + 4 > data.len() { return Err("Data too short".to_string()); }
        let num_nodes = u32::from_le_bytes(data[offset..offset+4].try_into().unwrap()) as usize;
        offset += 4;

        for _ in 0..num_nodes {
            if offset + 2 > data.len() { return Err("Data too short".to_string()); }
            let name_len = u16::from_le_bytes(data[offset..offset+2].try_into().unwrap()) as usize;
            offset += 2;
            if offset + name_len > data.len() { return Err("Data too short".to_string()); }
            let name = String::from_utf8_lossy(&data[offset..offset+name_len]).to_string();
            offset += name_len;

            let position = if offset + 12 <= data.len() {
                litt_math::Vec3::new(
                    f32::from_le_bytes(data[offset..offset+4].try_into().unwrap()),
                    f32::from_le_bytes(data[offset+4..offset+8].try_into().unwrap()),
                    f32::from_le_bytes(data[offset+8..offset+12].try_into().unwrap()),
                )
            } else { litt_math::Vec3::ZERO };
            offset += 12;

            let rotation = if offset + 16 <= data.len() {
                [
                    f32::from_le_bytes(data[offset..offset+4].try_into().unwrap()),
                    f32::from_le_bytes(data[offset+4..offset+8].try_into().unwrap()),
                    f32::from_le_bytes(data[offset+8..offset+12].try_into().unwrap()),
                    f32::from_le_bytes(data[offset+12..offset+16].try_into().unwrap()),
                ]
            } else { [0.0, 0.0, 0.0, 1.0] };
            offset += 16;

            let scale = if offset + 12 <= data.len() {
                litt_math::Vec3::new(
                    f32::from_le_bytes(data[offset..offset+4].try_into().unwrap()),
                    f32::from_le_bytes(data[offset+4..offset+8].try_into().unwrap()),
                    f32::from_le_bytes(data[offset+8..offset+12].try_into().unwrap()),
                )
            } else { litt_math::Vec3::new(1.0, 1.0, 1.0) };
            offset += 12;

            let parent_id = if offset + 4 <= data.len() {
                u32::from_le_bytes(data[offset..offset+4].try_into().unwrap())
            } else { !0u32 };
            offset += 4;

            let node_id = scene.create_node(&name, if parent_id != !0u32 { Some(parent_id) } else { None });
            if let Some(node) = scene.nodes.get_mut(&node_id) {
                node.position = position;
                node.rotation = rotation;
                node.scale = scale;
            }
        }

        Ok(scene)
    }
}
