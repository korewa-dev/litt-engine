# NOTES - skyline-run

2.5D momentum platformer: physics-driven gap arcs, drone patrols over floating platforms, checkpoint flags, gem side-paths, mast banner finish.

## Generation parameters (deterministic)

- geometry: platformer25d corridor, cyberpunk neon sunset - seed 909
- props: gen_props.py --kit platformer (merged palette, no-clobber)
- gameplay: enrich_game.py --brief brief.json --seed 91
- validation: play_native.py --frames 30 --dummy (interactives > 0 required)
- engine CI: native validation - littcli validate (zero missing models required)

## Controls (native player)

WASD move | Space jump | Q/E rotate camera | R respawn | Esc quit
