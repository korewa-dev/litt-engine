//! Scene node — single entity in the scene graph.

use litt_math::Vec3;
use std::collections::HashMap;

/// Scene node
#[derive(Debug)]
pub struct SceneNode {
    pub name: String,
    pub id: u32,
    pub parent: Option<u32>,
    pub children: Vec<u32>,
    pub position: Vec3,
    pub rotation: [f32; 4], // quaternion
    pub scale: Vec3,
    pub visible: bool,
    pub layer: u32,
    pub tags: Vec<String>,
}

impl SceneNode {
    pub fn new(name: &str, id: u32) -> Self {
        Self {
            name: name.to_string(),
            id,
            parent: None,
            children: Vec::new(),
            position: Vec3::ZERO,
            rotation: [0.0, 0.0, 0.0, 1.0],
            scale: Vec3::new(1.0, 1.0, 1.0),
            visible: true,
            layer: 0,
            tags: Vec::new(),
        }
    }

    pub fn add_child(&mut self, child_id: u32) {
        self.children.push(child_id);
    }

    pub fn remove_child(&mut self, child_id: u32) {
        self.children.retain(|&id| id != child_id);
    }

    pub fn add_tag(&mut self, tag: &str) {
        if !self.tags.contains(&tag.to_string()) {
            self.tags.push(tag.to_string());
        }
    }

    pub fn has_tag(&self, tag: &str) -> bool {
        self.tags.contains(&tag.to_string())
    }
}
