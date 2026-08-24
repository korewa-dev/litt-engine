# NOTES - ember-depths

2D top-down roguelite arena survivor: six escalating waves of Ember Wraiths and Ash Brutes, four brazier checkpoints, gem economy, spike hazards.

## Generation parameters (deterministic)

- geometry: bullet_hell/arena_ring, dark_fantasy night - seed 77
- props: gen_props.py --kit survivor (merged palette, no-clobber)
- gameplay: enrich_game.py --brief brief.json --seed 77
- validation: play_native.py --frames 30 --dummy (interactives > 0 required)
- engine CI: cargo test --test example_worlds (zero missing models required)

## Controls (native player)

WASD move | Space jump | Q/E rotate camera | R respawn | Esc quit
