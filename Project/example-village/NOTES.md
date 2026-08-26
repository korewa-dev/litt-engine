# example-village - NOTES

A complete WORKED EXAMPLE of a Litt game project. Copy this folder structure
(or just this whole directory) when the human asks for a separate new game.

## How this world was made (reproduce exactly)

    python template/tools/worldgen/gen_archetype.py \
        --archetype life_sim --pattern hub_spoke \
        --theme medieval_realism --time-of-day noon \
        --seed 42 --out-dir Project/example-village \
        --agent <your-name> --prompt "<the human request>"

- archetype: life_sim (hub_spoke_world structure per design_rules.json)
- pattern: hub_spoke - central plaza + 5 spoke paths + POI markers
- theme: medieval_realism (timber/plaster/thatch palette from themes.json)
- seed: 42 -> same bytes on every re-run

Supersedes the old procedural_assets.py demo (farm_house/cottage/pines);
those remain in git history.

## Layout

    assets/asset_index.json   manifest - read FIRST
    assets/models/*.obj|mtl   layout_main + poi_01..05
    assets/scenes/world.lscn.json  node graph with model:<name> tags
    ENGINE.bat / ENGINE.sh    native Vulkan player launchers
    VIEW.bat                  C++ orbit viewer (littview window)
    VALIDATE.bat              headless sim smoke (play_native.py)
    ATTRIBUTION.md            provenance rows

## Play

Run ENGINE.bat (Vulkan player) or `litt view example-village` for the C++
viewer. Movement mode resolves to top-down from state.identity.camera =
isometric; WASD walks the villager between POIs.

## TODOs for whoever copies this

- pick another archetype/pattern/theme triple from genre_index.csv, or copy
  gen_custom.py and edit its PALETTE / geometry / GAMEPLAY sections
- register any hand-made assets in asset_index.json + ATTRIBUTION.md
