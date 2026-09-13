# NOTES - worldforge-demo (WorldForge fusion)

- built by: world_forge.py (CDR-011), schema litt.worldforge/1
- spec seed: 5 | spawn region: r01-egyptian-desert
- objective: 1. start: spawn at 'r01-egyptian-desert' (egyptian_desert), then head on 2. middle: cross 'r02-tropical-island' (tropical_island walking_simulator) 3. finale: final objective at 'r03-desert-dunes' (desert_dunes dungeon_crawler): complete the finale
- links: r01-egyptian-desert->r02-tropical-island, r02-tropical-island->r03-desert-dunes -> 4 portal gate nodes
- duplicate player/start nodes stripped from non-spawn regions: 2
- gates: lint clean | littcli validate ok:true | native proof: skipped
- play: ENGINE.bat/.sh (Vulkan player) | VIEW.bat (C++ viewer)

## Regions

| region | generator | archetype/pattern | theme | role | origin | seed | nodes | objs |
|---|---|---|---|---|---|---|---|---|
| r01-egyptian-desert | archetype | open_world_survival/hub_spoke | egyptian_desert | start | [0,0,0] | 5 | 59 | 15 |
| r02-tropical-island | archetype | walking_simulator/corridor_run | tropical_island | middle | [70,0,0] | 6 | 44 | 9 |
| r03-desert-dunes | archetype | dungeon_crawler/room_graph | desert_dunes | finale | [140,0,0] | 7 | 38 | 10 |
