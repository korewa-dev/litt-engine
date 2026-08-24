# NOTES - kingsfall-hollow

3D soulslike hub: five shrines around a foggy courtyard, Hollow Knight guardians, garden stalkers, three kindleable bonfires with corpse-run rules, estus secrets behind shrines.

## Generation parameters (deterministic)

- geometry: soulslike/hub_spoke, haunted_estate dusk fog - seed 666
- props: gen_props.py --kit souls (merged palette, no-clobber)
- gameplay: enrich_game.py --brief brief.json --seed 666
- validation: play_native.py --frames 30 --dummy (interactives > 0 required)
- engine CI: cargo test --test example_worlds (zero missing models required)

## Controls (native player)

WASD move | Space jump | Q/E rotate camera | R respawn | Esc quit
