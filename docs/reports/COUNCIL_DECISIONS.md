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
