# WorldForge Spec Contract — `litt.worldforge/1`

> Council decision **CDR-011**: WorldForge = prompt -> spec -> multi-region
> open world, modeled on Tencent's WorldClaw (arXiv 2608.05248) planning half.
> One open-ended phrase becomes an explicit, editable structured world spec.
>
> Owner tools:
> - **Planner** (writes specs): `template/tools/worldgen/world_planner.py`
> - **Composer** (consumes specs, CDR-011 item 2): `template/tools/worldgen/world_forge.py`
>
> **Regeneration from an edited spec is THE feature.** The planner's output is
> a starting point, not a verdict: humans and agents hand-edit the JSON and the
> downstream pipeline honors every field.

---

## 1. Pipeline position

```
human phrase ("a frozen kingdom with a volcanic arena")
        |
        v
world_planner.py --about ... --seed S      [deterministic keyword mapping]
        |
        v
world_spec.json  <-- HAND-EDITABLE (this schema) --> world_planner.py --spec-in (validate)
        |
        v
world_forge.py <spec>                      [composer: per-region generators,
        |                                   merge at origins, portals on links]
        v
fused game dir -> lint -> littcli validate -> native proof (CDR-009 gates)
```

The planner NEVER generates geometry. The composer NEVER re-plans. The spec
JSON in the middle is the single source of truth both sides share.

## 2. Example document

```json
{
  "about": "a frozen kingdom with a volcanic arena",
  "name": "frost-realm",
  "objective_chain_hint": [
    "1. start: spawn at 'r01-arctic-expanse' (arctic_expanse), then head to 'r02-sky-islands'",
    "2. middle: cross 'r02-sky-islands' (sky_islands / open_world_survival), then head to 'r03-dark-fantasy'",
    "3. finale: final objective at 'r03-dark-fantasy' (dark_fantasy / character_action): complete the finale"
  ],
  "regions": [
    {
      "archetype": "open_world_rpg",
      "generator": "archetype",
      "id": "r01-arctic-expanse",
      "links": ["r02-sky-islands"],
      "origin": [0, 0, 0],
      "pattern": "hub_spoke",
      "role": "start",
      "size": 64,
      "theme": "arctic_expanse"
    },
    {
      "archetype": "open_world_survival",
      "generator": "archetype",
      "id": "r02-sky-islands",
      "links": ["r03-dark-fantasy"],
      "origin": [90, 0, 0],
      "pattern": "hub_spoke",
      "role": "middle",
      "size": 64,
      "theme": "sky_islands"
    },
    {
      "archetype": "character_action",
      "generator": "archetype",
      "id": "r03-dark-fantasy",
      "links": [],
      "origin": [190, 0, 0],
      "pattern": "arena_ring",
      "role": "finale",
      "size": 52,
      "theme": "dark_fantasy"
    }
  ],
  "schema": "litt.worldforge/1",
  "seed": 7,
  "spawn_region": "r01-arctic-expanse"
}
```

(Actual planner output is `json.dumps(..., sort_keys=True, indent=2)`; key
order above is illustrative.)

## 3. Field reference — top level

| Field                  | Type     | Req | Rules |
|------------------------|----------|-----|-------|
| `schema`               | string   | yes | Exactly `"litt.worldforge/1"`. Future revisions bump the suffix; consumers must reject unknown schemas. |
| `name`                 | string   | yes | Slug for the fused game/Project dir: `^[a-z0-9][a-z0-9-]{0,47}$`. Planner default: slugified `--about`, else `--name`. |
| `about`                | string   | yes | The original human phrase, verbatim. Non-empty. Kept for provenance and re-planning diffs. |
| `seed`                 | int      | yes | `>= 0` (bool is not an int). Drives all composer-side procedural choices; recorded in NOTES.md by convention. |
| `regions`              | array    | yes | 2..5 region objects, each unique `id`. Planner emits them in chain order (start -> middles -> finale); order in the array is NOT semantic - links carry topology. |
| `spawn_region`         | string   | yes | Must equal the `id` of the one region whose `role` is `"start"`. Composer places the player node here. |
| `objective_chain_hint` | array    | yes | Exactly one free-text string per region (`len == len(regions)`), written in traversal order. Hints feed brief authoring (objectives/side objectives); they are prose, not parsed. |

Unknown top-level keys are validation errors (typo protection for hand edits).

## 4. Field reference — region object

| Field       | Type        | Req | Rules |
|-------------|-------------|-----|-------|
| `id`        | string      | yes | Unique within spec; `^[a-z0-9][a-z0-9_-]{0,63}$`. Referenced by `links` and `spawn_region`. Planner format: `rNN-<slug>` where slug comes from theme/archetype. |
| `generator` | string      | yes | One of `soulslike \| space \| tabletop \| platformer25d \| archetype`. Selects which shipped generator the composer runs for this region: `gen_soulslike.py`, `gen_space.py`, `gen_tabletop.py`, `gen_platformer25d.py`, or `gen_archetype.py --archetype A --pattern P --theme T`. |
| `archetype` | string/null | cond | REQUIRED iff `generator == "archetype"`; must be a key of `design_rules.json -> archetypes` (76 entries). MUST be absent or null for every other generator (flagships take no archetype). |
| `pattern`   | string/null | cond | Only meaningful for `generator == "archetype"`; otherwise absent/null. When present: one of `arena_ring, corridor_run, hub_spoke, grid_board, spline_track, room_graph` (`PATTERNS` in gen_archetype.py). When absent, the composer derives one from the archetype's `structure` via the STRUCTURE_HINTS mapping; planner always writes it explicitly. |
| `theme`     | string      | yes | Key of `themes.json -> themes` (26 palettes+prop recipes). Drives materials/props for the region. |
| `role`      | string      | yes | `start \| middle \| finale`. Exactly one `start` and exactly one `finale` per spec; all others `middle` (0..3). |
| `origin`    | [x,y,z]     | yes | Three numbers (planner emits ints, y=0). World-space center of the region in the FUSED world. Must satisfy the spacing rule (§6). |
| `links`     | array       | yes | Region ids this region connects TO (directed edge, portal pair placed by composer). Each target must exist, no self-links, no duplicates. Planner builds the chain start->...->finale here; `--loop` adds finale->start. |
| `size`      | int         | yes | Footprint radius estimate in meters, clamped 24..140. Used ONLY for layout spacing (§6); generators may ignore it. |

## 5. Vocabulary sources of truth

| Vocabulary | File | Notes |
|------------|------|-------|
| generators (5)   | fixed list in schema | maps 1:1 onto shipped generator scripts |
| archetypes (76)  | `template/tools/worldgen/design_rules.json` | keys of `.archetypes`; identity blocks copied into state |
| patterns (6)     | `template/tools/worldgen/gen_archetype.py` `PATTERNS` | also the `Pattern_Or_Generator` column of genre_index.csv when it is not a script name |
| themes (26)      | `template/tools/worldgen/themes.json` | keys of `.themes` (never `_readme`) |
| genre rows       | `template/tools/worldgen/genre_index.csv` | canonical (archetype, pattern, suggested_theme) triples used by the mapper |

## 6. Layout & size heuristics (planner)

**Size estimate.** `size = clamp(base[generator] + delta[pattern], 24, 140)`
with base `{soulslike:72, space:96, tabletop:48, platformer25d:44, archetype:60}`
and delta `{arena_ring:-8, spline_track:+16, room_graph:-10, corridor_run:-6,
hub_spoke:+4, grid_board:-14}`.

**Origins.** Regions sit on the ground plane (`y = 0`). With K regions:

- `K == 4`: diamond - start west `(-S,0,0)`, middles north `(0,0,-S)` and
  south `(0,0,S)`, finale east `(S,0,0)`.
- otherwise: straight line along +X in chain order, starting at `(0,0,0)`,
  each gap `= ceil(halfsum(prev,next)/10)*10 + 10`.

`S` for the diamond is chosen as the smallest multiple of 10 such that ALL
pairwise distances satisfy the **spacing rule**:

```
for every pair (i, j):  dist(origin_i, origin_j) >= (size_i + size_j) / 2
```

This guarantees adjacent regions never overlap even if both generate to their
full estimated radius. The validator enforces the rule directly (V024), so any
hand-moved origin that violates it fails `--spec-in`.

## 7. Determinism contract

Same `--about` + `--seed` (+ same flags) => byte-identical spec file, across
processes and machines. Implementation rules: no wall-clock, no `hash()` of
strs (PYTHONHASHSEED), RNG seeded from `sha256("worldforge|<about>|<seed>")`,
all vocabulary iteration over sorted/fixed orders, `json.dumps(sort_keys=True,
indent=2)` + trailing newline. Omitting `--seed` uses the fixed default `1337`,
so even default runs reproduce byte-for-byte.

## 8. Validation rules (`--spec-in`)

Exit 0 prints a report plus machine-readable last line
`{"ok":true,...}`; exit 1 lists every violation as human-readable lines plus
`{"ok":false,"violations":[...]}`. Codes:

| Code | Rule |
|------|------|
| V001 | top-level JSON object; file parses |
| V002 | `schema == "litt.worldforge/1"` |
| V003 | `name` matches slug regex |
| V004 | `about` non-empty string |
| V005 | `seed` int >= 0 (not bool) |
| V006 | `regions` array with 2..5 entries |
| V010 | region `id` regex + uniqueness |
| V011 | `generator` in the 5-value set |
| V012 | `theme` exists in themes.json |
| V013 | `role` in {start, middle, finale} |
| V014 | `origin` = list of exactly 3 numbers |
| V015 | `size` int within 24..140 |
| V016 | `links` targets exist, no self, no duplicates |
| V017 | `archetype` required iff generator==archetype; value in design_rules.json; forbidden otherwise |
| V018 | `pattern` only for generator==archetype; value in the 6-pattern set |
| V020 | exactly one role==start |
| V021 | exactly one role==finale |
| V022 | `spawn_region` equals the start region id |
| V023 | `objective_chain_hint` = list of strings, len == len(regions) |
| V024 | pairwise origin spacing >= half-sum of sizes (§6) |
| V025 | directed reachability: every region reachable from `spawn_region` over `links` |
| V026 | no unknown keys (top level or region level) |

## 9. Hand-edit workflow

1. `python template/tools/worldgen/world_planner.py --about "<phrase>" --seed S --out world_spec.json`
2. Edit `world_spec.json` in any text editor. Common recipes:
   - **Swap a region's look:** change its `theme` to another themes.json key.
     Regeneration re-dresses exactly that region (CDR-011 gate).
   - **Change a gameplay feel:** swap `archetype`/`pattern` (keep generator ==
     archetype), e.g. `hub_spoke` -> `arena_ring`.
   - **Grow/shrink the world:** add/remove middle regions; keep 2..5 total,
     relink into the chain, keep spacing rule, extend/trim
     `objective_chain_hint` to match.
   - **Move regions:** edit `origin`s; respect V024 (validator tells you the
     minimum distance for the pair you broke).
   - **Reorder the journey:** rewrite `links`; any directed graph reachable
     from `spawn_region` is legal, chains and loops included.
3. Lint the edit: `python world_planner.py --spec-in world_spec.json` (exit 0 = good).
4. Compose: `python world_forge.py world_spec.json` (CDR-011 item 2) runs the
   five-region pipeline: namespaced scratch generation per region, asset/node
   prefixing, placement at `origin`, paired portal/goal-gate nodes on every
   `links` edge, spawn in `spawn_region`, objective chain from
   `objective_chain_hint`, then lint + littcli + native proof gates.

Because the spec is explicit, ANY tool in the repo (or agent) can read it and
know the intended world without reverse-engineering prompts.

## 10. Machine-readable IO conventions

- Planner success: last stdout line is
  `{"ok": true, "spec": "<path>", "name": "...", "seed": N, "regions": ["id", ...]}`.
- Validate success/failure: last stdout line is
  `{"ok": true|false, "spec": "<path>", "violations": [...]}`;
  process exit code 0 or 1 respectively.
- All output UTF-8, ASCII-escaped JSON, LF newlines.
