//! Scene graph — hierarchical collection of nodes.

use super::scene_node::SceneNode;

/// Scene graph
#[derive(Debug)]
pub struct SceneGraph {
    pub nodes: HashMap<u32, SceneNode>,
    pub root_id: u32,
    pub next_id: u32,
}

impl Default for SceneGraph {
    fn default() -> Self { Self::new() }
}

impl SceneGraph {
    /// Create a new empty scene graph
    pub fn new() -> Self {
        let root = SceneNode::new("Root", 0);
        Self {
            nodes: vec![root].into_iter().enumerate().map(|(i, n)| (i as u32, n)).collect(),
            root_id: 0,
            next_id: 1,
        }
    }

    /// Create a new node
    pub fn create_node(&mut self, name: &str, parent_id: Option<u32>) -> u32 {
        let id = self.next_id;
        self.next_id += 1;
        let mut node = SceneNode::new(name, id);
        if let Some(parent) = parent_id {
            node.parent = Some(parent);
            if let Some(parent_node) = self.nodes.get_mut(&parent) {
                parent_node.add_child(id);
            }
        }
        self.nodes.insert(id, node);
        id
    }

    /// Get a node reference
    pub fn get(&self, id: u32) -> Option<&SceneNode> {
        self.nodes.get(&id)
    }

    /// Get a mutable node reference
    pub fn get_mut(&mut self, id: u32) -> Option<&mut SceneNode> {
        self.nodes.get_mut(&id)
    }

    /// Remove a node and all children
    pub fn remove(&mut self, id: u32) -> bool {
        if let Some(node) = self.nodes.remove(&id) {
            // Remove from parent
            if let Some(parent_id) = node.parent {
                if let Some(parent) = self.nodes.get_mut(&parent_id) {
                    parent.remove_child(id);
                }
            }
            // Remove children
            for child_id in node.children {
                self.remove(child_id);
            }
            true
        } else {
            false
        }
    }

    /// Find node by name
    pub fn find_by_name(&self, name: &str) -> Option<u32> {
        self.nodes.iter().find(|(_, n)| n.name == name).map(|(id, _)| *id)
    }

    /// Get all nodes in the scene
    pub fn all_nodes(&self) -> Vec<&SceneNode> {
        self.nodes.values().collect()
    }

    /// Get visible nodes
    pub fn visible_nodes(&self) -> Vec<&SceneNode> {
        self.nodes.values().filter(|n| n.visible).collect()
    }

    /// Get nodes by layer
    pub fn nodes_by_layer(&self, layer: u32) -> Vec<&SceneNode> {
        self.nodes.values().filter(|n| n.layer == layer).collect()
    }

    /// Get nodes by tag
    pub fn nodes_by_tag(&self, tag: &str) -> Vec<&SceneNode> {
        self.nodes.values().filter(|n| n.has_tag(tag)).collect()
    }
}
