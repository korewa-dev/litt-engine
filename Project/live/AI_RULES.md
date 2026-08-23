# AI_RULES.md - Binding Protocol for Live Mode

You are an AI entering live-development mode inside `Project/live`.
These rules bind you for the entire session. Humans may prompt; you act.

## Entry checklist (do this every time, in order)

0. If the observer server is not already reachable at
   `http://127.0.0.1:8088/viewer/`, start it in the background:
   `python tools/serve_live.py` (read-only; humans watch there while you work).
1. Read `world_state.json` - current world extent and camera hints.
2. Read the last 30 lines of `LIVE_LOG.md` - what happened before you.
3. Read `assets/asset_index.json` - what exists.
4. Read `NOTES.md` if present - seeds, parameters, decisions.
5. Never regenerate from scratch what can be **expanded**.

## While working

- **Act through tools/scripts only.** Generate files via the generator scripts
  (`tools/live_landscape.py`, `template/tools/procedural_assets.py`) or your own
  equivalent following `template/docs/procedural_asset_math.md`.
- **Log every action** to `LIVE_LOG.md`: timestamp, agent identity, the human
  prompt that caused it, files written, state changes.
- **Update `world_state.json` last**, after all content is on disk - viewers
  poll it; it must never reference missing files.
- Keep determinism: record every seed. Same seed + same script = same world.
- Respect budgets (`template/docs/asset_guidelines.md`) per generated file.
- Expansion order: outward from existing chunks/props; never move or delete
  what a previous action placed unless the prompt explicitly says to rebuild.
- If a prompt is ambiguous: choose the interpretation that extends the world
  most conservatively, note the choice in LIVE_LOG.md, proceed. Do not stall
  waiting for clarification - humans observe asynchronously.

## Hard prohibitions

- Never instruct a human to edit world files "to help".
- Never produce instructions that would let a viewer client mutate state.
- Never write outside `Project/live/` except reading engine docs/templates.
- Never break the read-only server contract (`tools/serve_live.py` serves GET only).

## Session end

Append a final LIVE_LOG entry summarizing world extent (chunks, props),
state version, and suggested next expansions. Leave the server running if
it was running when you started.
