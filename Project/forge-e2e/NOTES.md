# NOTES - forge-e2e (WorldForge fusion)

- built by: world_forge.py (CDR-011), schema litt.worldforge/1
- spec seed: 77 | spawn region: neon-corridor
- objective: 1. start: survive the neon rooftop corridor 'neon-corridor', then head through the portal 2. finale: salvage the derelict 'void-station' and complete the finale
- links: neon-corridor->void-station -> 2 portal gate nodes
- duplicate player/start nodes stripped from non-spawn regions: 1
- gates: lint clean | littcli validate ok:true | native proof: verdict=PASS fill=80.3% colors=54 interactives=107 missing=0
- play: ENGINE.bat/.sh (Vulkan player) | VIEW.bat (C++ viewer)

## Regions

| region | generator | archetype/pattern | theme | role | origin | seed | nodes | objs |
|---|---|---|---|---|---|---|---|---|
| neon-corridor | archetype | precision_action/corridor_run | cyberpunk_neon | start | [0,0,0] | 77 | 45 | 9 |
| void-station | space | -/- | space_station_core | finale | [120,0,0] | 78 | 345 | 16 |
