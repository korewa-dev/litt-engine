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
    viewer/                   play.html + runtime.js + three.min.js (vendored)
    tools/serve_live.py       read-only server + player host
    PLAY.bat                  double-click to play (port 8089)
    ATTRIBUTION.md            provenance rows

## Play

Double-click PLAY.bat (or: python tools/serve_live.py --port 8089 then open
http://127.0.0.1:8089/viewer/play.html). Movement mode resolves to top-down
from state.identity.camera = isometric; WASD walks the villager between POIs.

## TODOs for whoever copies this

- pick another archetype/pattern/theme triple from genre_index.csv, or copy
  gen_custom.py and edit its PALETTE / geometry / GAMEPLAY sections
- register any hand-made assets in asset_index.json + ATTRIBUTION.md
