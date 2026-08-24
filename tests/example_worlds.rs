//! Every shipped game in `Project/` must load and deploy natively.
//!
//! This is the engine's contract with its AI builders: a generated world that
//! passes the Python toolchain MUST also survive the Rust pipeline
//! (`litt-scene` parse -> areas -> world_bridge OBJ deployment) with zero
//! missing models. If you add a game folder, add it to GAMES below.

use litt::world_bridge::build_render_scene;
use std::path::Path;

const GAMES: &[&str] = &["ember-depths", "kingsfall-hollow", "skyline-run"];

#[test]
fn example_worlds_deploy_natively() {
    let mut checked = 0usize;
    for game in GAMES {
        let dir = format!("Project/{game}");
        let asset_dir = format!("{dir}/assets");
        let scene_path = format!("{asset_dir}/scenes/world.lscn.json");
        if !Path::new(&scene_path).exists() {
            panic!("game {game} is missing its scene file ({scene_path})");
        }

        let (graph, areas) = litt::load_graph_and_areas_file(&scene_path)
            .unwrap_or_else(|e| panic!("game {game}: scene failed to parse: {e}"));

        assert!(
            graph.all_nodes().len() > 3,
            "game {game}: scene has almost no nodes"
        );

        // Areas are optional but must be well-formed when present.
        for area in &areas {
            assert!(area.radius > 0.0, "game {game}: area {} bad radius", area.name);
            assert!(!area.name.is_empty());
        }

        // The editor snapshot API must accept real generated nodes.
        if let Some(node) = graph.get(1) {
            let summary = litt::editor::NodeSummary::from(node);
            assert!(!summary.name.is_empty());
        }

        let (scene, stats) = build_render_scene(&graph, &asset_dir);
        assert!(
            stats.missing_models.is_empty(),
            "game {game}: missing models {:?}",
            stats.missing_models
        );
        assert!(
            scene.triangles.len() > 100,
            "game {game}: deployed scene is nearly empty ({} tris)",
            scene.triangles.len()
        );
        println!(
            "[native-test] {game}: {} tris, {} areas, {} meshes",
            scene.triangles.len(),
            areas.len(),
            stats.meshes_loaded
        );
        checked += 1;
    }
    assert_eq!(checked, GAMES.len(), "not all games were checked");
}
