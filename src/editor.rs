//! Editor session -- scene inspection and manipulation for humans AND AI.
//!
//! [`EditorSession`] is deliberately headless: every operation is a plain
//! method on plain data, so unit tests (and AI agents driving the engine
//! through code) can build worlds without a window or GPU. The interactive
//! `litt edit <scene>` mode in `main.rs` is a thin key-binding shell around
//! this core; the file-based workflow (`save`, JSON diffs) stays the primary
//! AI entry point.

use litt_math::Vec3;
use litt_scene::{SceneGraph, SceneNode};

/// What the editor did after an operation (for HUD/log feedback).
#[derive(Clone, Debug, PartialEq)]
pub enum EditOutcome {
    /// Selection moved to a new node id
    Selected(Option<u32>),
    /// Named node created with id
    Created(u32),
    /// Node removed
    Deleted(u32),
    /// Node transform changed
    Moved(u32),
    /// Scene written to disk
    Saved(String),
    /// Operation could not be performed
    Rejected(String),
    /// Nothing to do
    None,
}

/// Headless scene editor over a [`SceneGraph`].
pub struct EditorSession {
    pub graph: SceneGraph,
    /// Path the session saves to
    pub path: String,
    pub selected: Option<u32>,
    /// World-units per nudge step
    pub step: f32,
    /// Dirty flag: unsaved edits exist
    pub dirty: bool,
}

impl EditorSession {
    /// Open an existing scene file.
    pub fn open(path: &str) -> Result<Self, String> {
        let graph = litt_scene::load_graph_file(path)?;
        Ok(Self {
            graph,
            path: path.to_string(),
            selected: None,
            step: 1.0,
            dirty: false,
        })
    }

    /// Start an empty scene bound to a save path.
    pub fn new_scene(path: &str) -> Self {
        Self {
            graph: SceneGraph::new(),
            path: path.to_string(),
            selected: None,
            step: 1.0,
            dirty: false,
        }
    }

    // -- selection ---------------------------------------------------------

    /// Cycle selection through nodes in deterministic (id) order, skipping Root.
    pub fn select_next(&mut self) -> EditOutcome {
        let mut ids: Vec<u32> = self.graph.nodes.keys().copied().filter(|&i| i != self.graph.root_id).collect();
        ids.sort_unstable();
        if ids.is_empty() {
            self.selected = None;
            return EditOutcome::Selected(None);
        }
        let next = match self.selected {
            None => ids[0],
            Some(cur) => {
                let pos = ids.iter().position(|&i| i == cur).map(|p| p + 1).unwrap_or(0);
                ids[pos % ids.len()]
            }
        };
        self.selected = Some(next);
        EditOutcome::Selected(Some(next))
    }

    /// Select by exact node name.
    pub fn select_by_name(&mut self, name: &str) -> EditOutcome {
        match self.graph.find_by_name(name) {
            Some(id) if id != self.graph.root_id => {
                self.selected = Some(id);
                EditOutcome::Selected(Some(id))
            }
            _ => EditOutcome::Rejected(format!("no node named '{}'", name)),
        }
    }

    // -- creation / deletion -------------------------------------------------

    /// Create a node under root at a position; selects it. Tags optional.
    pub fn add_node(&mut self, name: &str, pos: [f32; 3], tags: &[&str]) -> EditOutcome {
        if name.is_empty() {
            return EditOutcome::Rejected("node name must not be empty".into());
        }
        if self.graph.find_by_name(name).is_some() {
            return EditOutcome::Rejected(format!("node '{}' already exists", name));
        }
        let id = self.graph.create_node(name, Some(self.graph.root_id));
        {
            let n = self.graph.get_mut(id).unwrap();
            n.position = Vec3::new(pos[0], pos[1], pos[2]);
            for t in tags {
                n.add_tag(t);
            }
        }
        self.selected = Some(id);
        self.dirty = true;
        EditOutcome::Created(id)
    }

    /// Delete the selected node (and its children, via SceneGraph::remove).
    pub fn delete_selected(&mut self) -> EditOutcome {
        match self.selected.take() {
            Some(id) => {
                if self.graph.remove(id) {
                    self.dirty = true;
                    EditOutcome::Deleted(id)
                } else {
                    EditOutcome::Rejected(format!("node {} not found", id))
                }
            }
            None => EditOutcome::Rejected("nothing selected".into()),
        }
    }

    // -- transforms ------------------------------------------------------------

    /// Move the selected node by a world-space delta.
    pub fn translate_selected(&mut self, delta: [f32; 3]) -> EditOutcome {
        let id = match self.selected {
            Some(id) => id,
            None => return EditOutcome::Rejected("nothing selected".into()),
        };
        let n = self.graph.get_mut(id).unwrap();
        n.position.0 += delta[0];
        n.position.1 += delta[1];
        n.position.2 += delta[2];
        self.dirty = true;
        EditOutcome::Moved(id)
    }

    /// Nudge along one axis: 0=x, 1=y, 2=z (sign from `positive`).
    pub fn nudge(&mut self, axis: u8, positive: bool) -> EditOutcome {
        let s = self.step * if positive { 1.0 } else { -1.0 };
        let mut d = [0.0f32; 3];
        d[axis as usize % 3] = s;
        self.translate_selected(d)
    }

    /// Set uniform scale of the selected node.
    pub fn scale_selected(&mut self, uniform: f32) -> EditOutcome {
        let id = match self.selected {
            Some(id) => id,
            None => return EditOutcome::Rejected("nothing selected".into()),
        };
        if !(uniform > 0.0 && uniform.is_finite()) {
            return EditOutcome::Rejected("scale must be positive and finite".into());
        }
        let n = self.graph.get_mut(id).unwrap();
        n.scale = Vec3::new(uniform, uniform, uniform);
        self.dirty = true;
        EditOutcome::Moved(id)
    }

    /// Rename the selected node (uniqueness enforced).
    pub fn rename_selected(&mut self, new_name: &str) -> EditOutcome {
        let id = match self.selected {
            Some(id) => id,
            None => return EditOutcome::Rejected("nothing selected".into()),
        };
        if new_name.is_empty() || self.graph.find_by_name(new_name).is_some() {
            return EditOutcome::Rejected(format!("invalid or duplicate name '{}'", new_name));
        }
        self.graph.get_mut(id).unwrap().name = new_name.to_string();
        self.dirty = true;
        EditOutcome::Moved(id)
    }

    // -- areas ---------------------------------------------------------------

    /// Wrap the selected node as an area marker with radius (scale.x*10 convention).
    pub fn mark_selected_as_area(&mut self, radius: f32, tags: &[&str]) -> EditOutcome {
        let id = match self.selected {
            Some(id) => id,
            None => return EditOutcome::Rejected("nothing selected".into()),
        };
        if !(radius > 0.0 && radius.is_finite()) {
            return EditOutcome::Rejected("radius must be positive".into());
        }
        {
            let n = self.graph.get_mut(id).unwrap();
            n.add_tag("area");
            for t in tags {
                n.add_tag(t);
            }
            n.scale = Vec3::new(radius / 10.0, 1.0, 1.0);
        }
        self.dirty = true;
        EditOutcome::Moved(id)
    }

    // -- persistence -----------------------------------------------------------

    /// Save to the session path.
    pub fn save(&mut self) -> EditOutcome {
        match litt_scene::save_graph_file(&self.graph, &self.path) {
            Ok(()) => {
                self.dirty = false;
                EditOutcome::Saved(self.path.clone())
            }
            Err(e) => EditOutcome::Rejected(e),
        }
    }

    /// One-line status for HUD / logs / AI transcripts.
    pub fn status_line(&self) -> String {
        let sel = match self.selected.and_then(|id| self.graph.get(id)) {
            Some(n) => format!(
                "{} @ ({:.1},{:.1},{:.1}) scale {:.2} tags {:?}",
                n.name, n.position.0, n.position.1, n.position.2, n.scale.0, n.tags
            ),
            None => "<none>".to_string(),
        };
        format!(
            "[{}] {} nodes | sel: {}",
            if self.dirty { "UNSAVED" } else { "saved" },
            self.graph.nodes.len() - 1,
            sel
        )
    }

    /// Read-only view of a node (AI-friendly snapshot without live refs).
    pub fn describe(&self, id: u32) -> Option<NodeSummary> {
        self.graph.get(id).map(|n| NodeSummary {
            name: n.name.clone(),
            id: n.id,
            position: [n.position.0, n.position.1, n.position.2],
            scale: [n.scale.0, n.scale.1, n.scale.2],
            visible: n.visible,
            tags: n.tags.clone(),
        })
    }
}

/// Owned summary of a node -- safe to log, send, or serialize.
#[derive(Clone, Debug, PartialEq)]
pub struct NodeSummary {
    pub name: String,
    pub id: u32,
    pub position: [f32; 3],
    pub scale: [f32; 3],
    pub visible: bool,
    pub tags: Vec<String>,
}

impl From<&SceneNode> for NodeSummary {
    fn from(n: &SceneNode) -> Self {
        Self {
            name: n.name.clone(),
            id: n.id,
            position: [n.position.0, n.position.1, n.position.2],
            scale: [n.scale.0, n.scale.1, n.scale.2],
            visible: n.visible,
            tags: n.tags.clone(),
        }
    }
}

// =============================================================================
// Interactive shell (windowed mode for humans; AI keeps using the file API)
// =============================================================================

const EDITOR_HELP: &str = "Tab select-next | Arrows move X/Z | PgUp/PgDn Y | +/- step | N new node | A mark area | P deploy | Del delete | Ctrl+S save | Esc quit";

/// Run the windowed editor session for a scene file (created when missing).
#[cfg(target_os = "windows")]
pub fn run_interactive(path: &str) -> Result<(), String> {
    let mut session = if std::path::Path::new(path).exists() {
        EditorSession::open(path)?
    } else {
        let s = EditorSession::new_scene(path);
        println!("New scene: {}", path);
        s
    };

    let mut window = litt_platform::Window::new("Litt Editor", litt_platform::WindowSize::default())
        .ok_or_else(|| "Failed to create editor window".to_string())?;
    let mut input = litt_input::InputSystem::new();
    let mut overlay = litt_ui::Overlay::new();
    let mut node_counter = 1u32;

    println!("{}", EDITOR_HELP);
    loop {
        window.pump_messages();
        if window.should_close() {
            break;
        }
        let events = window.take_events();
        input.ingest_platform(&events);

        let st = input.state();
        let ctrl_held = st.key_down(litt_input::Key::LControl);

        // Selection
        if st.key_pressed(litt_input::Key::Tab) {
            report(session.select_next());
        }
        // Movement
        if st.key_pressed(litt_input::Key::ArrowRight) { report(session.nudge(0, true)); }
        if st.key_pressed(litt_input::Key::ArrowLeft) { report(session.nudge(0, false)); }
        if st.key_pressed(litt_input::Key::PageUp) { report(session.nudge(1, true)); }
        if st.key_pressed(litt_input::Key::PageDown) { report(session.nudge(1, false)); }
        if st.key_pressed(litt_input::Key::ArrowUp) { report(session.nudge(2, true)); }
        if st.key_pressed(litt_input::Key::ArrowDown) { report(session.nudge(2, false)); }
        // Step size
        if st.key_pressed(litt_input::Key::Equals) {
            session.step = (session.step * 2.0).min(64.0);
            println!("step -> {:.2}", session.step);
        }
        if st.key_pressed(litt_input::Key::Minus) {
            session.step = (session.step / 2.0).max(0.0625);
            println!("step -> {:.2}", session.step);
        }
        // Create / classify / delete
        if st.key_pressed(litt_input::Key::N) {
            let name = format!("Node_{:03}", node_counter);
            node_counter += 1;
            report(session.add_node(&name, [0.0, 0.5, 0.0], &[]));
        }
        if st.key_pressed(litt_input::Key::A) {
            report(session.mark_selected_as_area(20.0, &["section"]));
        }
        if st.key_pressed(litt_input::Key::Delete) {
            report(session.delete_selected());
        }
        // Deploy: run the native world->render-scene conversion on demand.
        // This is the same path the GPU pipeline consumes; it validates the
        // world's assets end-to-end (meshes found, transforms applied).
        if st.key_pressed(litt_input::Key::P) {
            let base = if std::path::Path::new("assets/models").is_dir() { "assets" } else { "." };
            let (_, stats) = crate::world_bridge::build_render_scene(&session.graph, base);
            println!(
                "[deploy] tris={} markers={} meshes={} missing={:?}",
                stats.triangles, stats.spheres, stats.meshes_loaded, stats.missing_models
            );
        }
        // Save
        if ctrl_held && st.key_pressed(litt_input::Key::S) {
            report(session.save());
        }
        // Quit (saves unsaved work rather than losing it)
        if st.key_pressed(litt_input::Key::Escape) {
            if session.dirty {
                report(session.save());
            }
            break;
        }

        // Status overlay (text primitives; GPU text pipeline renders them later)
        overlay.clear();
        overlay.draw_text(&session.status_line(), 12.0, 12.0, [0.95, 0.95, 0.9, 1.0], 16.0);
        overlay.draw_text(EDITOR_HELP, 12.0, 34.0, [0.6, 0.6, 0.62, 1.0], 13.0);
        if let Some(id) = session.selected {
            if let Some(n) = session.describe(id) {
                overlay.draw_text(
                    &format!("sel {} ({})", n.name, n.id),
                    12.0,
                    56.0,
                    [1.0, 0.62, 0.25, 1.0],
                    15.0,
                );
            }
        }

        input.end_frame();
    }
    Ok(())
}

#[cfg(not(target_os = "windows"))]
pub fn run_interactive(_path: &str) -> Result<(), String> {
    Err("interactive editor requires a supported desktop platform".to_string())
}

fn report(outcome: EditOutcome) {
    match outcome {
        EditOutcome::None => {}
        other => println!("{:?}", other),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn session() -> EditorSession {
        let mut s = EditorSession::new_scene("unused_tmp.lscn.json");
        s.add_node("Crate", [0.0, 0.0, 0.0], &["prop"]);
        s.add_node("Lamp", [4.0, 2.0, -1.0], &["light"]);
        s
    }

    #[test]
    fn create_select_cycle_delete() {
        let mut s = session();
        assert_eq!(s.select_next(), EditOutcome::Selected(Some(1)));
        assert_eq!(s.select_next(), EditOutcome::Selected(Some(2)));
        assert_eq!(s.select_next(), EditOutcome::Selected(Some(1))); // wraps

        assert_eq!(s.delete_selected(), EditOutcome::Deleted(1));
        assert_eq!(s.select_by_name("Crate"), EditOutcome::Rejected("no node named 'Crate'".into()));
        assert_eq!(s.select_by_name("Lamp"), EditOutcome::Selected(Some(2)));
    }

    #[test]
    fn moves_scale_rename_guarded() {
        let mut s = session();
        s.select_by_name("Crate");
        assert_eq!(s.nudge(0, true), EditOutcome::Moved(1));
        assert_eq!(s.describe(1).unwrap().position[0], 1.0);
        assert_eq!(s.scale_selected(2.5), EditOutcome::Moved(1));
        assert_eq!(s.describe(1).unwrap().scale, [2.5, 2.5, 2.5]);
        assert!(matches!(s.scale_selected(-1.0), EditOutcome::Rejected(_)));

        s.select_by_name("Lamp");
        assert!(matches!(s.rename_selected("Crate"), EditOutcome::Rejected(_))); // dup
        assert_eq!(s.rename_selected("Torch"), EditOutcome::Moved(2));
    }

    #[test]
    fn area_marking_feeds_area_system() {
        use litt_scene::areas::AreaSystem;

        let mut s = session();
        s.select_by_name("Crate");
        assert_eq!(s.mark_selected_as_area(30.0, &["music:calm"]), EditOutcome::Moved(1));

        let areas = AreaSystem::from_tagged_nodes(&s.graph);
        assert_eq!(areas.len(), 1);
        assert_eq!(areas.area_at([1.0, 0.0, 0.0]).unwrap().name, "Crate");
        assert!(areas.all()[0].tags.contains(&"music:calm".to_string()));
    }

    #[test]
    fn save_and_reload_roundtrip() {
        let dir = std::env::temp_dir().join("litt_editor_test");
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("edit_roundtrip.lscn.json");
        let path_str = path.to_str().unwrap();

        let mut s = EditorSession::new_scene(path_str);
        s.add_node("Hero", [1.0, 2.0, 3.0], &["player"]);
        assert!(matches!(s.save(), EditOutcome::Saved(_)));
        assert!(!s.dirty);

        let mut reopened = EditorSession::open(path_str).unwrap();
        assert_eq!(reopened.select_by_name("Hero"), EditOutcome::Selected(Some(1)));
        assert_eq!(reopened.describe(1).unwrap().position, [1.0, 2.0, 3.0]);
        let _ = std::fs::remove_file(&path);
    }

    #[test]
    fn status_line_reports_dirty_state() {
        let mut s = session();
        assert!(s.status_line().contains("UNSAVED"));
        assert!(s.status_line().contains("sel:"));
    }
}
