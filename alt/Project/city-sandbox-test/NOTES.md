# NOTES - city-sandbox-test

Original open-world city action test sandbox for Litt Engine. This is a technical test project, not a copy of GTA V assets, map, characters, story, dialogue, or branding.

## Runtime test contract

This project only claims systems implemented by the current `litt_game.h` runtime:

- third-person free-roam movement
- jump/gravity and ground collision
- enemy contact reset
- pickup scoring
- goal completion

The following are tracked as engine gaps and are intentionally not faked through metadata: driveable vehicles, traffic simulation, wanted/police logic, weapon combat, mission state machines, and pedestrian AI.

## Test route

Spawn -> collect Street_Pickup_A -> collect Street_Pickup_B -> avoid Street_Hazard_A/B -> reach Sandbox_Goal.

- baseline: Litt Engine open_world_realistic generated world
- pattern=hub_spoke theme=modern_city_day seed=1001
- play: ENGINE.bat/.sh
- inspect: VIEW.bat
- validate: repository verify_project.py / native littcli when available
