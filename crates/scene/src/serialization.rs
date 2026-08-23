//! Scene serialization — save/load scene graphs as JSON.
//!
//! AI-agent-friendly: scenes round-trip through human-readable JSON so
//! agents can diff, generate, and edit worlds with plain text tools.
//!
//! ```ignore
//! use litt_scene::serialization::*;
//! let json = save_graph_json(&graph)?;
//! std::fs::write("level1.lscn.json", &json)?;
//! let graph = load_graph_json(&std::fs::read_to_string("level1.lscn.json")?)?;
//! ```

use serde::{Deserialize, Serialize};
use std::path::Path;

use crate::scene_graph::SceneGraph;
use crate::scene_node::SceneNode;

/// Serializable scene node DTO (math types flattened to arrays).
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct NodeDto {
    pub name: String,
    pub id: u32,
    pub parent: Option<u32>,
    pub children: Vec<u32>,
    pub position: [f32; 3],
    pub rotation: [f32; 4],
    pub scale: [f32; 3],
    pub visible: bool,
    pub layer: u32,
    pub tags: Vec<String>,
}

/// Serializable scene graph DTO.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct SceneDto {
    /// Format magic, always "litt-scene"
    pub format: String,
    /// Format version
    pub version: u32,
    pub root_id: u32,
    pub next_id: u32,
    pub nodes: Vec<NodeDto>,
}

impl SceneDto {
    pub const FORMAT: &'static str = "litt-scene";
    pub const VERSION: u32 = 1;
}

impl From<&SceneNode> for NodeDto {
    fn from(n: &SceneNode) -> Self {
        Self {
            name: n.name.clone(),
            id: n.id,
            parent: n.parent,
            children: n.children.clone(),
            position: [n.position.0, n.position.1, n.position.2],
            rotation: n.rotation,
            scale: [n.scale.0, n.scale.1, n.scale.2],
            visible: n.visible,
            layer: n.layer,
            tags: n.tags.clone(),
        }
    }
}

impl From<&NodeDto> for SceneNode {
    fn from(d: &NodeDto) -> Self {
        let mut node = SceneNode::new(&d.name, d.id);
        node.parent = d.parent;
        node.children = d.children.clone();
        node.position = litt_math::Vec3::new(d.position[0], d.position[1], d.position[2]);
        node.rotation = d.rotation;
        node.scale = litt_math::Vec3::new(d.scale[0], d.scale[1], d.scale[2]);
        node.visible = d.visible;
        node.layer = d.layer;
        node.tags = d.tags.clone();
        node
    }
}

impl From<&SceneGraph> for SceneDto {
    fn from(g: &SceneGraph) -> Self {
        let mut nodes: Vec<NodeDto> = g.nodes.values().map(NodeDto::from).collect();
        // Deterministic ordering by id so diffs are stable
        nodes.sort_by_key(|n| n.id);
        Self {
            format: Self::FORMAT.to_string(),
            version: Self::VERSION,
            root_id: g.root_id,
            next_id: g.next_id,
            nodes,
        }
    }
}

impl From<&SceneDto> for SceneGraph {
    fn from(d: &SceneDto) -> Self {
        let mut graph = SceneGraph::new();
        graph.nodes.clear();
        for node_dto in &d.nodes {
            graph.nodes.insert(node_dto.id, SceneNode::from(node_dto));
        }
        graph.root_id = d.root_id;
        graph.next_id = d.next_id;
        graph
    }
}

/// Serialize a scene graph to pretty JSON text.
pub fn save_graph_json(graph: &SceneGraph) -> Result<String, String> {
    let dto = SceneDto::from(graph);
    serde_json::to_string_pretty(&dto).map_err(|e| format!("Scene JSON serialize failed: {}", e))
}

/// Deserialize a scene graph from JSON text.
pub fn load_graph_json(json: &str) -> Result<SceneGraph, String> {
    let dto: SceneDto = serde_json::from_str(json)
        .map_err(|e| format!("Scene JSON parse failed: {}", e))?;
    if dto.format != SceneDto::FORMAT {
        return Err(format!("Bad scene format '{}' (expected '{}')", dto.format, SceneDto::FORMAT));
    }
    Ok(SceneGraph::from(&dto))
}

/// Save a scene graph to a `.json` file.
pub fn save_graph_file(graph: &SceneGraph, path: &str) -> Result<(), String> {
    let json = save_graph_json(graph)?;
    std::fs::write(Path::new(path), json)
        .map_err(|e| format!("Scene write '{}' failed: {}", path, e))
}

/// Load a scene graph from a `.json` file.
pub fn load_graph_file(path: &str) -> Result<SceneGraph, String> {
    let json = std::fs::read_to_string(Path::new(path))
        .map_err(|e| format!("Scene read '{}' failed: {}", path, e))?;
    load_graph_json(&json)
}

#[cfg(test)]
mod tests {
    use super::*;
    use litt_math::Vec3;

    fn sample_graph() -> SceneGraph {
        let mut g = SceneGraph::new();
        let a = g.create_node("Player", Some(g.root_id));
        let b = g.create_node("Camera", Some(a));
        g.get_mut(a).unwrap().position = Vec3::new(1.0, 2.0, 3.0);
        g.get_mut(a).unwrap().add_tag("player");
        g.get_mut(b).unwrap().visible = false;
        g.get_mut(b).unwrap().layer = 2;
        g
    }

    #[test]
    fn roundtrip_json() {
        let g = sample_graph();
        let json = save_graph_json(&g).unwrap();
        let g2 = load_graph_json(&json).unwrap();
        assert_eq!(g2.next_id, g.next_id);
        assert_eq!(g2.nodes.len(), g.nodes.len());
        let cam = g2.find_by_name("Camera").unwrap();
        assert!(!g2.get(cam).unwrap().visible);
        assert_eq!(g2.get(cam).unwrap().layer, 2);
        let player = g2.find_by_name("Player").unwrap();
        assert_eq!(g2.get(player).unwrap().position, Vec3::new(1.0, 2.0, 3.0));
        assert!(g2.get(player).unwrap().has_tag("player"));
    }

    #[test]
    fn roundtrip_file() {
        let g = sample_graph();
        let dir = std::env::temp_dir().join("litt_scene_test");
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("roundtrip.lscn.json");
        let path_str = path.to_str().unwrap();
        save_graph_file(&g, path_str).unwrap();
        let g2 = load_graph_file(path_str).unwrap();
        assert_eq!(g2.nodes.len(), g.nodes.len());
        let _ = std::fs::remove_file(&path);
    }

    #[test]
    fn rejects_bad_format() {
        let r = load_graph_json("{\"format\":\"nope\"}");
        assert!(r.is_err());
    }
}
