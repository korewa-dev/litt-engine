# CITY SANDBOX RUNTIME AUDIT

## Scope
Audit of `alt/Project/city-sandbox-test` against the current native `litt_game.h` behavior.

## Verified runtime capabilities
- Scene/model loading from the project directory.
- Player spawn from semantic `Player_Start`.
- WASD/arrow free-roam movement.
- Jump, gravity, ground collision, and fall reset.
- `enemy` contact resets the player.
- `pickup` / `score` interaction awards score and consumes the node.
- `goal` / `win` interaction sets the won state.

## Corrected claim
`identity.movement=vehicle_movement` was metadata only. The current Game runtime does not branch into a vehicle controller. The test project now declares `free_roam_movement`, matching actual behavior.

## Explicit engine gaps
Driveable vehicles, traffic simulation, wanted/police logic, weapon combat, mission state machines, and pedestrian AI require engine implementation before this project may claim them.

## Acceptance
The sandbox scene contains concrete pickup, enemy/hazard, and goal nodes using model assets already present in its asset index. No external or copied game assets are introduced.
