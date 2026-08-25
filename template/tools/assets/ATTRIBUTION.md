# Asset Attribution - worldgen tooling (template scope)

Provenance for shared kit assets authored by `template/tools/worldgen/`
tooling itself (per-game copies are attributed in each
`Project/<name>/ATTRIBUTION.md`, deployed by `make_game.py`).

| Asset(s) | Source | License |
|---|---|---|
| gen_props.py kits survivor/platformer/souls (coin, gem, heart, brazier, wraith, brute, spike, checkpoint_flag, drone, banner, estus_flask, bonfire, stalker, knight) | Procedural primitives via `worldkit.MeshBuilder`; math-only, no external input | Repo-original (CC0-equivalent in-repo asset) |
| gen_props.py **shared kit v2** (goal_gate, fog_veil, hazard_pit, hazard_spikes, hex_pawn, token_gem, star_glint, asteroid_small/medium/large, platform_deck_short/mid/long, ruin_arch, ruin_pillar) - ASSET_AUDIT fix 1.1 | Procedural re-creations at origin of shapes previously hand-rolled inside gen_space / gen_soulslike / gen_platformer25d / gen_tabletop; silhouettes match within tolerance | Repo-original |
| gen_props.py palettes (`PALETTES`, fix 1.3) | Derived at import from `template/tools/worldgen/themes.json` (canonical prop vocabulary aliased onto each theme's own palette keys) | Repo-original data |
