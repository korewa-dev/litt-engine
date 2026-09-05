# Engine Council — Decision Records

> Standing body for the litt engine rewrite. Decides what goes in and what
> stays out. Weighted seats; verdicts recorded below with rationale.

**Seat weights:** lead w3 · gfx w2 · qa w1 (mirrors `native/council_demo.cpp`)
**Quorum:** 0.5 default — a motion needs yes-weight > no-weight AND turnout ≥ quorum

---

## CDR-001 — Staged C/C++ rewrite of the engine core

**Verdict:** SOUND-WITH-RISKS (unanimous)
**Motion:** Proceed with staged rewrite: C core → C++17 header modules → editor/viewer.
**Rationale:** Order is sound; risk concentrates in Stage 3 (viewer/editor) where
API surface churns fastest. Mitigation: umbrella-header syntax gate + test suite
before any UI layer work.
**Status:** ADOPTED. Evidence: native/ builds clean, tests.cpp 23/23 PASS,
benchmarks show Mat4 mul at 0.70x raw cost (lib faster than hand-rolled floats).

## CDR-002 — Header-only C++17 modules under native/littcore/

**Verdict:** ADOPTED (unanimous)
**Motion:** Keep all engine modules as self-contained headers; no .cpp in core.
**Conditions (all satisfied):**
1. Every header must survive `g++ -std=c++17 -fsyntax-only` via `litt.h`.
2. No query/mutator name collisions on one class (Input press/release lesson).
3. Distinct names for same-named concepts across headers (World ECS vs world sim;
   Config game rules vs Settings store).
**Status:** ENFORCED going forward. New modules failing the gate stay out.

## CDR-003 — Council pattern for feature load/unload

**Verdict:** ADOPTED (gfx: yes w2, qa: abstain → carried by lead proxy vote)
**Motion:** Runtime feature loading decided by weighted vote with tier presets
and manual override escape hatch (`litt_council.h`).
**Notes:** Quorum blocking is a feature, not a bug (see council_demo: audio
motion blocked at turnout 1/6 even with a yes). Overrides are logged decisions,
not silent bypasses.
**Status:** SHIPPED + self-tested.

## CDR-004 — Worldgen rebuild executed by agent offload

**Verdict:** IN PROGRESS
**Motion:** Rebuild template/tools/worldgen generators per goal: multi-asset
composition, collision-safe placement, tag-driven gameplay wiring, deterministic
seeds, verified end-to-end through make_game.py for soulslike / space /
tabletop / platformer25d and gen_archetype dispatch.
**Gate to adopt:** flagship generators pass littcli validation + native_proof
pixel assertions, lint-clean output, reproducible from fixed seeds.
**Status:** IN EXECUTION. Scout audit complete (WORLDGEN_AUDIT.md): 22-item
punch list; key findings — gen_soulslike.py has a fatal SyntaxError (truncated
file), make_game never invokes the three flagship generators (only
gen_archetype + gen_platformer25d), algokit.py has zero importers, and the
double-transform bug class is systemic. Wave 1 dispatched: worldkit placement
registry + transform convention (items 1-2, prerequisites) and soulslike repair
(item 12) in parallel on disjoint files.
**Gate evidence (independently verified, see CLAIM_VERIFICATION.md):**
`build.bat test` = 21 green · `native_proof.py` = 3/3 games pixel-PASS ·
`verify_project.py` = 0 failing. The gates are real and currently green; the
rebuild must keep them green after every generator lands.

---

*New decisions append below this line. One CDR per motion; never edit history.*

---

## CDR-005 — Native core audit verdict: NEEDS FIXES before asset pass

**Verdict:** ADOPTED (core auditor + lead)
**Motion:** Fix C1 (OBJ no-trailing-newline NULL-deref, empirically proven
segfault) and M1–M3 (mode-resolution divergence from gameplay.rs contract,
astral-plane UTF-8 corruption) BEFORE any real-world worldgen asset pass.
**Evidence:** docs/reports/CORE_AUDIT.md — 1 CRITICAL, 3 MAJOR, 8 MINOR, 12 NIT;
rasterizer delta math independently verified exact; JSON buffer safety verified
sound. Regression tests added to tests.c so the fixed contracts stay locked.
**Status:** IN EXECUTION (agent ac128239). Gate: build.bat test all-green
including new vectors.

## CDR-006 — Worldgen rebuild wave plan

**Verdict:** ADOPTED (lead, per user directive to offload to agents)
**Motion:** Execute the 22-item punch list in dependency waves on disjoint
files: Wave 1 = prerequisites (worldkit registry/transform convention) +
broken-file repair (soulslike SyntaxError); Wave 2 = archetype gameplay wiring;
Wave 3 = make_game collision-aware placement + native_proof gate + flagship
rebuilds. Final gate: full pipeline for soulslike/space/tabletop/platformer25d
via gen_archetype dispatch, littcli validate clean, native_proof pixel-PASS,
byte-reproducible seeds.
**Status:** IN EXECUTION (agents 6346bbe0, c447bf04, a779cfff).

---

## CDR-007 — Remove the Rust + web stacks; C/C++ becomes THE runtime

**Verdict:** ADOPTED (user decision: "replace all html and rust stuff as it
sucks"; HTML/web already fully removed in commit 5bd235f)
**Motion:** Delete `src/` (22 Rust files), `crates/` (114 .rs files across 15
crates), workspace Cargo.toml/lock, and every build/config reference to cargo,
rustc, and wasm. The C/C++ port under native/littcore is already contract-
equivalent for everything littcli/littview consume: gameplay.rs mode resolution
(ported + regression-tested this session in CDR-005's fix wave), physics
constants, JSON/OBJ/world IO. Rust remains as REFERENCE ONLY during the
transition window: extract any still-unported semantics into C++ first, then
delete. No Rust toolchain may be required to build, test, or validate after
this lands.
**Gates to adopt:** (1) grep-clean: no Cargo.toml/Cargo.lock/.rs tracked, no
cargo/rustc/wasm references outside docs/reports/ history; (2) native gates
still green after removal: build.bat test all-pass, verify_project.py,
native_proof.py pixel-PASS on shipped games; (3) template pipeline unaffected:
make_game.py end-to-end ok for one probe per flagship kind.
**Status:** QUEUED behind Wave 3 completion (agents db0ec1b4, 71cea1e9,
3cc703f6 own the working tree areas that share verify gates). Deletion agent
dispatches when they close.

## CDR-008 — North star: WorldClaw-style agentic world generation

**Verdict:** ADOPTED as long-term direction (user goal statement)
**Reference:** Tencent Hunyuan3D "WorldClaw" (arXiv 2608.05248) — agentic,
coarse-to-fine open-world generation: planning agents turn a prompt into a
structured spec of regions/terrain/assets/materials/spatial-relations; a
globally coherent terrain foundation with region-aware height field;
terrain-conditioned compositions with editable instance-level assets;
render-based agents refine terrain, objects, appearance, contacts.
**Mapping onto litt engine (staged, no new languages):**
1. NOW (Wave 3): make_game --kind dispatch + native proof gate = the seed of
   the spec->generate->verify loop.
2. NEXT: region planner (prompt -> regions/biomes spec JSON) feeding gen_space/
   gen_soulslike terrain fields; worldkit Placement registry scales to region
   adjacency; height-field query API (worldkit.query_height) becomes the
   terrain-conditioned placement substrate.
3. THEN: render-based refinement agents — littview render + bmp_stats pixel
   assertions are already the "render-based critic"; iterate generate -> render
   -> assert -> regenerate until pixel gates pass.
4. Asset kits grow toward editable instance-level composition (instancing work
   from items 15/19-21 is the base pattern).
Everything above runs on the existing stack: Python worldgen orchestrators +
C/C++ core + littcli/littview validation. No HTML, no Rust, no browser.

**Progress log:** Item 16 (space collision-safe scatter + goal semantics) DONE
and lead-verified: exactly 1 jump_gate goal node, >=5 pickup salvage pods (6),
hub + 24 hazards preserved, 280 stars intact, overlap checker reports zero
skipped placements with registry re-roll, lint clean, littcli ok:true
(32 interactives), byte-deterministic.
Items 4+5+6 (room_graph BFS-guaranteed connectivity via plan_room_connectivity/
room_reachability, spline control-point conditioning, hub_spoke POI/coin
origin-centered rebuild) DONE, lead-verified across all three patterns:
validate ok:true (Side2D5 + Orbit3D), reachability proofs 3/3 and 5/5 across
seeds in the wave report, byte-deterministic.
Items 7/8/9 (make_game --kind dispatch for all five kinds, native proof gate
with --skip-native-proof hatch, Placement-registry prop placement) DONE and
lead-verified END-TO-END: soulslike/space/tabletop/platformer25d/archetype +
--about regression all exit 0 with machine-readable JSON incl. native_proof
(fill 4.49-81.16%, colors 27-44, interactives 42-63, missing 0).
WAVE 3 COMPLETE. Remaining punch-list work continues under CDR-009 doctrine.

---

## CDR-009 — Asset creation doctrine: WorldClaw-aligned (binding)

**Verdict:** ADOPTED (user directive: asset creation should follow Tencent
Hunyuan3D WorldClaw, arXiv 2608.05248)
**Doctrine — every asset-creation change from now on must satisfy all five:**
1. **Reusable kit pieces** — assets come from versionable kits (gen_props.py),
   never one-off bespoke meshes buried inside a generator.
2. **Instance-level editability** — meshes are origin-centered and placed via
   node transforms (worldkit convention); no baked multi-object compositions;
   every placed thing remains individually movable/removable in the scene JSON.
3. **Terrain-conditioned placement** — placement queries the height field
   (worldkit.query_height) and the Placement registry so assets sit ON terrain,
   never floating/clipping/overlapping.
4. **Procedural materials** — palette/theme-driven material assignment
   (mat_at convention), generative variation seeded deterministically; no
   binary texture dependencies required for validation.
5. **Render-based refinement** — after generation, littcli validate +
   littview render + bmp_stats pixel assertions act as the critic loop;
   assets that fail visual gates (fill/colors/span) get regenerated or fixed.
**Status:** Scout dispatched to audit gen_props.py + kit consumers against the
five principles and produce docs/reports/ASSET_AUDIT.md punch list.
**Progress log:** ASSET_AUDIT.md delivered (P1 2/5, P2 3/5, P3 2/5, P4 2/5,
P5 2/5; two renderer-proven double-transforms; materials never reach pixels).
Kit v2 DONE + lead-verified: 35 pieces / 4 kits (new shared kit absorbs
asteroids, fog_veil, goal_gate, hazard_pit/spikes, platform decks, pawns,
gems, star_glint, ruin arch/pillar), --seed variation deterministic within
silhouette bounds, PALETTES replaced by themes.json vocabulary, all pieces
origin-tolerant, build_prop signature backwards compatible.
Foundation mirrors cleaned (lead): all 14 rule files de-Rust'd per CDR-007;
AGENTS.md self-description corrected to "C/C++ + Python worldgen"; stale
live_landscape path fixed to Project/live/tools/.

---

## CDR-010 — Agentic refine loop (WorldClaw principle 5 closure)

**Verdict:** ADOPTED (user directive: the AI agent needs the WorldClaw
generate->critique->REFINE capability or AI-driven game development is not
viable).
**Motion:** Close the open critic loop: a deterministic, bounded
generate -> prove -> diagnose -> re-seed -> redeploy orchestrator
(refine_game.py) wrapping make_game + native_proof.proof_one_game:
1. Attempt k uses seed derived from base seed (documented sequence).
2. Each attempt is scored (fill %, colors, rows span, sim ok, missing=0,
   optional yaw-delta) into a composite.
3. First fully-passing attempt deploys to Project/ and logs all attempts to
   NOTES.md; otherwise after N attempts the BEST-scoring candidate ships to
   scratch with diagnostics and exit 1.
4. Failure diagnosis names the failed assertions with margins and lists
   suspect assets (largest tri-count models in underfilled bands).
   Per-pixel per-asset attribution is deferred until littview gains a
   hide-node render flag (noted as v2).
5. Last stdout line is machine-readable JSON (attempt trail included).
**Gates:** passing-path E2E <=N attempts exit 0; forced-fail path ships best
candidate + exits 1 with trail; determinism of attempt sequence; make_game
standalone path unregressed.

---

## CDR-011 — WorldForge: prompt -> spec -> multi-region open world

**Verdict:** ADOPTED (user directive after WorldClaw analysis: "build
something similar").
**Analysis:** WorldClaw = agentic framework turning one open-ended prompt
into an explicit, explorable, editable open-world 3D scene. Its pipeline:
planning agents produce a structured specification of REGIONS/terrain/assets/
spatial-relations; globally coherent foundation; terrain-conditioned regional
compositions; render-based refinement. Litt owns everything below planning;
CDR-010 adds refinement. WorldForge adds the missing top.
**Motion:** Two new tools sharing one explicit spec contract
(docs/specs/WORLDFORGE_SPEC.md, schema litt.worldforge/1):
1. `world_planner.py --about "<any phrase>"` — planning agent: deterministic
   keyword/genre mapping (genre_index.csv + design_types.json) decomposes the
   prompt into 2-5 regions {id, generator, archetype/pattern/theme, origin,
   role: start|middle|finale, links} -> writes world_spec.json. Editable BY
   HAND: regeneration reads the edited spec (explicit + editable).
2. `world_forge.py <spec>` — composer: runs each region's generator into
   namespaced scratch, merges into ONE game dir (prefixed assets/nodes,
   regions placed at spec origins, paired portal/goal gate nodes on links,
   spawn in start region, objective chain across regions), lint + littcli +
   native proof gates on the fused world.
**Gates:** E2E demo prompt ("a frozen kingdom with a volcanic arena") ->
spec -> fused world passes validate ok:true AND native proof PASS AND is
playable via ENGINE launcher; determinism same-seed byte-stable; edited spec
(e.g. swap a theme) changes exactly that region.
**Integration (user directive: "integrate it to the game engine fully"):**
WorldForge is wired into every engine surface, not left as a side script —
`litt forge "<phrase>"` / `litt refine [...]` verbs on the main CLI,
a Forge button in the C# Studio GUI next to world cooking, command
documentation in AGENTS.md + mirrors. games.json registration contract must
hold for fused worlds so `litt status`/Studio list them like any game.
**Progress log:** Item 12 (soulslike SyntaxError repair) DONE and
lead-verified independently: py_compile passes; generation produces 35 chunks /
45 assets / 70 tagged nodes; lint clean; littcli validate ok:true (35 solids,
11 interactives); full gameplay tag contract present (enemy/boss/pickup/
checkpoint/player/fog-gate/corpse_run); byte-deterministic. Remaining:
item 13 (--seed plumbing), item 14 (kit-based rebuild).
Item 15 (space instancing) DONE and lead-verified: OBJ count 285+ -> 8 with
280 star instance nodes preserved; lint clean; littcli ok:true (29
interactives incl. 24 hazards + 5 salvage pods); byte-deterministic.
Remaining space work: item 16 (collision-safe scatter + goal semantics).
Item 3 (archetype gameplay wiring) DONE and lead-verified across all three
patterns: corridor_run now emits 10 pickup nodes + 1 goal node (was baked-in,
0 interactives -> now 11); arena_ring has enemies + goal; grid_board has
pieces + goal. All validate ok:true with interactives>0 and >=1 goal node.
Remaining: items 4-6 (room_graph connectivity, spline robustness, hub
double-transform).
