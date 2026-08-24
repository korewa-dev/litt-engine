//! Areas -- named spherical world regions ("sections") with membership,
//! tags, and player transition tracking.
//!
//! An area is a lightweight volume marker: center + radius + free-form tags
//! (music cues, environment overrides, gameplay modes, level names). Nodes can
//! be assigned to an area by id; the [`AreaSystem`] tracks which area a world
//! position falls in and reports enter/leave transitions each frame so games
//! and the AI can react (stream content, switch music, trigger events).
//!
//! ```ignore
//! use litt_scene::areas::{AreaDef, AreaSystem};
//! let mut areas = AreaSystem::new();
//! areas.register(AreaDef::new(1, "Market", [0.0, 0.0, 0.0], 25.0));
//! areas.update([30.0, 0.0, 0.0]); // -> None (outside)
//! areas.update([5.0, 0.0, 0.0]);  // -> entered "Market"
//! ```

use serde::{Deserialize, Serialize};

/// A named region of the world.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct AreaDef {
    /// Stable area id (unique within a scene)
    pub id: u32,
    /// Display name, e.g. "Old Town"
    pub name: String,
    /// World-space center of the activation sphere
    pub center: [f32; 3],
    /// Activation radius in world units
    pub radius: f32,
    /// Free-form classification / behavior tags
    pub tags: Vec<String>,
    /// Scene node ids that belong to this area
    #[serde(default)]
    pub nodes: Vec<u32>,
}

impl AreaDef {
    pub fn new(id: u32, name: &str, center: [f32; 3], radius: f32) -> Self {
        Self {
            id,
            name: name.to_string(),
            center,
            radius: radius.max(0.0),
            tags: Vec::new(),
            nodes: Vec::new(),
        }
    }

    pub fn with_tags(mut self, tags: &[&str]) -> Self {
        self.tags = tags.iter().map(|t| t.to_string()).collect();
        self
    }

    pub fn contains(&self, p: [f32; 3]) -> bool {
        let dx = p[0] - self.center[0];
        let dy = p[1] - self.center[1];
        let dz = p[2] - self.center[2];
        dx * dx + dy * dy + dz * dz <= self.radius * self.radius
    }
}

/// Reported when the occupied area changes.
#[derive(Clone, Debug, PartialEq)]
pub struct AreaTransition {
    /// Area just left, if any
    pub left: Option<u32>,
    /// Area just entered, if any
    pub entered: Option<u32>,
}

/// Tracks areas and the currently occupied one.
#[derive(Debug, Default)]
pub struct AreaSystem {
    areas: Vec<AreaDef>,
    current: Option<u32>,
}

impl AreaSystem {
    pub fn new() -> Self {
        Self::default()
    }

    /// Insert or replace an area by id.
    pub fn register(&mut self, area: AreaDef) {
        if let Some(slot) = self.areas.iter_mut().find(|a| a.id == area.id) {
            *slot = area;
        } else {
            self.areas.push(area);
        }
    }

    pub fn remove(&mut self, id: u32) -> bool {
        let before = self.areas.len();
        self.areas.retain(|a| a.id != id);
        if self.current == Some(id) {
            self.current = None;
        }
        self.areas.len() != before
    }

    pub fn len(&self) -> usize {
        self.areas.len()
    }

    pub fn is_empty(&self) -> bool {
        self.areas.is_empty()
    }

    pub fn get(&self, id: u32) -> Option<&AreaDef> {
        self.areas.iter().find(|a| a.id == id)
    }

    pub fn all(&self) -> &[AreaDef] {
        &self.areas
    }

    /// Currently occupied area id, if any.
    pub fn current(&self) -> Option<u32> {
        self.current
    }

    /// Smallest area containing `p`, regardless of occupancy tracking.
    pub fn area_at(&self, p: [f32; 3]) -> Option<&AreaDef> {
        // Prefer the tightest fit so nested areas behave intuitively.
        self.areas
            .iter()
            .filter(|a| a.contains(p))
            .min_by(|a, b| a.radius.partial_cmp(&b.radius).unwrap_or(std::cmp::Ordering::Equal))
    }

    /// Feed the current world position; returns a transition when the
    /// occupied area changed this call.
    pub fn update(&mut self, p: [f32; 3]) -> Option<AreaTransition> {
        let next = self.area_at(p).map(|a| a.id);
        if next == self.current {
            return None;
        }
        let t = AreaTransition {
            left: self.current,
            entered: next,
        };
        self.current = next;
        Some(t)
    }

    /// Build an area system from scene nodes tagged `"area"`: position is the
    /// center, `scale.x * 10` is the radius convention used by generators.
    pub fn from_tagged_nodes(graph: &crate::scene_graph::SceneGraph) -> Self {
        let mut sys = Self::new();
        for node in graph.nodes_by_tag("area") {
            let radius = (node.scale.0 * 10.0).max(1.0);
            sys.register(AreaDef {
                id: node.id,
                name: node.name.clone(),
                center: [node.position.0, node.position.1, node.position.2],
                radius,
                tags: node.tags.clone(),
                nodes: node.children.clone(),
            });
        }
        sys
    }
}

impl From<&crate::scene_graph::SceneGraph> for Vec<AreaDef> {
    fn from(_g: &crate::scene_graph::SceneGraph) -> Self {
        AreaSystem::from_tagged_nodes(_g).areas
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn containment_and_tightest_fit() {
        let outer = AreaDef::new(1, "Plaza", [0.0, 0.0, 0.0], 50.0);
        let inner = AreaDef::new(2, "Fountain", [0.0, 0.0, 0.0], 5.0);
        let mut sys = AreaSystem::new();
        sys.register(outer);
        sys.register(inner);

        assert_eq!(sys.area_at([40.0, 0.0, 0.0]).unwrap().name, "Plaza");
        assert_eq!(sys.area_at([1.0, 0.0, 0.0]).unwrap().name, "Fountain");
        assert!(sys.area_at([100.0, 0.0, 0.0]).is_none());
    }

    #[test]
    fn transitions_fire_once() {
        let mut sys = AreaSystem::new();
        sys.register(AreaDef::new(7, "Dungeon", [10.0, 0.0, 0.0], 8.0).with_tags(&["dark", "music:dungeon"]));

        assert!(sys.update([0.0, 0.0, 0.0]).is_none());
        let t = sys.update([12.0, 0.0, 0.0]).unwrap();
        assert_eq!(t.entered, Some(7));
        assert_eq!(t.left, None);
        // Staying inside produces no event.
        assert!(sys.update([13.0, 0.0, 0.0]).is_none());
        let t = sys.update([-50.0, 0.0, 0.0]).unwrap();
        assert_eq!(t.left, Some(7));
        assert_eq!(t.entered, None);
    }

    #[test]
    fn register_replaces_and_remove_works() {
        let mut sys = AreaSystem::new();
        sys.register(AreaDef::new(1, "A", [0.0, 0.0, 0.0], 5.0));
        sys.register(AreaDef::new(1, "A2", [1.0, 0.0, 0.0], 6.0));
        assert_eq!(sys.len(), 1);
        assert_eq!(sys.get(1).unwrap().name, "A2");
        assert!(sys.remove(1));
        assert!(!sys.remove(1));
        assert!(sys.is_empty());
    }

    #[test]
    fn serde_roundtrip() {
        let a = AreaDef::new(3, "Market", [4.0, 0.0, -2.0], 20.0).with_tags(&["music:market"]);
        let json = serde_json::to_string(&a).unwrap();
        let back: AreaDef = serde_json::from_str(&json).unwrap();
        assert_eq!(back, a);
        // Legacy files without "nodes" still parse.
        let legacy: AreaDef =
            serde_json::from_str("{\"id\":1,\"name\":\"X\",\"center\":[0,0,0],\"radius\":5,\"tags\":[]}").unwrap();
        assert!(legacy.nodes.is_empty());
    }

    #[test]
    fn vec3_center_matches_array() {
        use litt_math::Vec3;
        let c = Vec3(1.0, 2.0, 3.0);
        let a = AreaDef::new(9, "T", [c.0, c.1, c.2], 3.0);
        assert!(a.contains([2.0, 2.0, 3.0]));
        assert!(!a.contains([9.0, 2.0, 3.0]));
    }
}
