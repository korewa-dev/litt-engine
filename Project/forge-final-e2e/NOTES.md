# NOTES - forge-final-e2e (WorldForge fusion)

- built by: world_forge.py (CDR-011), schema litt.worldforge/1
- spec seed: 21 | spawn region: r01-haunted-estate
- objective: 1. start: spawn at 'r01-haunted-estate' (haunted_estate), then head on 2. middle: cross 'r02-underground-caves' (underground_caves dungeon_crawler) 3. finale: final objective at 'r03-sky-islands' (sky_islands character_action): complete the finale
- links: r01-haunted-estate->r02-underground-caves, r02-underground-caves->r03-sky-islands -> 4 portal gate nodes
- duplicate player/start nodes stripped from non-spawn regions: 2
- gates: lint clean | littcli validate ok:true | native proof: verdict=PASS fill=81.19% colors=57 interactives=106 missing=0
- play: ENGINE.bat/.sh (Vulkan player) | VIEW.bat (C++ viewer)

## Regions

| region | generator | archetype/pattern | theme | role | origin | seed | nodes | objs |
|---|---|---|---|---|---|---|---|---|
| r01-haunted-estate | archetype | naval_pirate/spline_track | haunted_estate | start | [0,0,0] | 21 | 36 | 10 |
| r02-underground-caves | archetype | dungeon_crawler/room_graph | underground_caves | middle | [80,0,0] | 22 | 36 | 10 |
| r03-sky-islands | archetype | character_action/arena_ring | sky_islands | finale | [150,0,0] | 23 | 39 | 11 |
