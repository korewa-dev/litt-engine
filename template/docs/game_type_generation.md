# Game Type Generation Guide

How an AI turns "make me a game like X" into a generated, PLAYABLE world.
This is the top-level entry point for game-type generation; asset-level detail
lives in ai_asset_creation.md.

Everything referenced here ships in the repo:

    template/tools/worldgen/     generators + identity/theme datasets
    native/littcore + littcli    the C runtime contract + headless validator
    template/tools/worldgen/algokit.py  algorithms as callable code

---

## The lookup chain - resolve ANY request in three hops

    human request ("a game like Hades")
      -> genre_index.csv          find the row (example-game column matches fuzzy asks)
      -> design_rules.json        confirm archetype -> camera/combat/movement/pattern
      -> ONE generator command    run it; done

genre_index.csv columns: Genre, Subgenre, Archetype_Key, Pattern_Or_Generator,
Suggested_Theme, Example_Games. All 76 archetypes are indexed and every key is
verified against design_rules.json.

---

## The four datasets (what each one answers)

| File | Question it answers |
|------|---------------------|
| design_types.json | vocabulary: cameras, combat styles, movement, structures, narrative, pacing, signatures |
| design_rules.json | per archetype: defaults for all dimensions + environment types + CONCRETE procgen rules + AI behavior + which generator |
| themes.json | per theme: palette (RGB), primitive prop recipes, environment notes |
| genre_index.csv | per example game: exact archetype key, pattern/generator to run, suggested theme |

---

## Generator fleet

One-command (preferred — dispatches the whole pipeline incl. native proof):

    python make_game.py --kind {soulslike|space|tabletop|platformer25d|archetype} \
        [--name N] [--seed S] [--archetype A] [--pattern P] [--theme T] [--about "phrase"]
    python make_game.py --random                      # fully random game
    # add --skip-native-proof to bypass the pixel gate; last stdout line is JSON

Flagships (bespoke, richer):

    python gen_soulslike.py      # bonfire / fog gate / boss arena / corpse run
    python gen_space.py          # station, asteroid field, pods, star canopy
    python gen_tabletop.py       # hex board, terrain bands, pawns, dice
    python gen_platformer25d.py  # jump-arc verified gaps, spikes, coins, goal

Universal (any of the 76 archetypes):

    python gen_archetype.py --archetype <key> [--pattern P] [--theme T]
                            [--time-of-day dawn|noon|dusk|night]
                            [--weather clear|rain|snow|fog|storm]
                            [--seed N] [--out-dir DIR] [--list]

Layout patterns (auto-picked from the archetype structure if you omit --pattern):

    arena_ring    boss arenas, bullet hell, fighting stages, MOBA pits
    corridor_run  runners, character action, walking sims, gauntlets
    hub_spoke     open worlds, RPGs, monster taming, collectathons
    grid_board    tabletop, tactics, deckbuilders, farming plots, match3 boards
    spline_track  racing, karts, sim racing, flight courses, tower defense roads
    room_graph    roguelikes, dungeons, horror wings, heists, detective scenes

Every run writes: OBJ assets + materials.mtl + asset_index.json +
assets/scenes/world.lscn.json + world_state.json (identity, gameplay,
environment blocks) + LIVE_LOG.md entry. Deterministic from --seed.

---

## Make it playable

make_game.py already ships the native launchers into every project:

    <project>/ENGINE.bat (or ./ENGINE.sh)   -> Vulkan player: `litt play <game>`
    <project>/VIEW.bat                      -> C++ orbit viewer: `litt view <game>`

The same contract the browser runtime used to honor now lives in
native/littcore/litt_world.c: it reads state.identity to choose the
movement mode (third-person 3D, side-scroller, top-down), applies
state.gameplay.physics verbatim (gravity, jump velocity, run speed, coyote
time), and node tags drive pickups/goals/checkpoints/hazards/enemies.
No conversion step exists - whatever you generate is immediately playable.
Prove it headless with `native/bin/littcli validate <project> --frames 60`.

---

## Worked examples

"Cozy island creature-collecting game":

    python gen_archetype.py --archetype monster_taming \
        --theme tropical_island --time-of-day noon --seed 12

"Kart racer like Mario Kart but candy-themed, sunset":

    python gen_archetype.py --archetype kart_racer --pattern spline_track \
        --theme candy_land --time-of-day dusk --seed 5

"Foggy night dungeon crawl":

    python gen_archetype.py --archetype roguelike --pattern room_graph \
        --time-of-day night --weather fog --seed 9

Then run `litt play <game>` to hand the human the world.

---

## Extending the system

Add an archetype: append it to design_extra.json (same schema as
design_rules.json entries) and run merge_rules.py. Fields: camera, combat,
movement, structure, narrative, pacing, environment_types, procgen_rules,
ai_behavior, generator.

Add a theme: append to themes.json - palette (5-7 named RGB triples), props
(primitive recipes as text), env_notes (atmosphere hints the runtime surfaces).

Add a flagship generator: copy gen_custom.py (heavily commented scaffold),
follow its TODOs; keep determinism (all randomness through worldkit Rng with a
recorded seed), write world_state.json LAST, log every run.

New algorithm: add it to algokit.py WITH assertions proving it works before
committing; document it in genre_algorithms.md.

---

## Hard rules (unchanged)

Determinism: same seed = same bytes; seeds recorded in logs.
Budgets: < 500 KB model, palette <= ~12 materials.
Registration: every asset lands in asset_index.json automatically - never
hand-write files behind the generator's back.
State LAST: viewers poll world_state.json; it must never reference missing files.
