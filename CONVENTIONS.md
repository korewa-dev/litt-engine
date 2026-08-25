<!-- REMOVED STACK NOTICE (CDR-007): The Rust engine described here was removed from the repo; this document remains as design reference for the C/C++ port (native/littcore). -->
# Litt Engine - AI Agent Rules (mirror of AGENTS.md)

Litt Engine is a game engine DRIVEN BY AI agents. Your job here: BUILD GAMES.

THE ONE RULE: never create, modify, rename or delete anything outside `Project/`
unless the human explicitly asked for an ENGINE change. Reading anything is fine;
`native/ studio/ shaders/ include/ docs/ template/ assets/`
are engine internals - not yours to touch.

THE TOOL-USAGE LAW (mandatory): before claiming something is missing, broken,
or non-existent, you MUST prove it with the tools actually present:
1. glob — discover what files exist (pattern search, not assumed name)
2. read — inspect actual file content
3. run — invoke the tool and observe real output
4. report — only then state what is present or absent
If you skip this and report a gap, you are guessing. Re-verify any gap
report from another agent before acting on it.

ROUTING:
- "develop" / "build a world/game" -> LIVE MODE: cd Project/live, read AI_RULES.md,
  watch: litt view live (native C++ orbit viewer)
  orient via world_state.json + LIVE_LOG.md, THEN expand via
  python tools/live_landscape.py --radius N --seed S (or any worldgen generator).
- separate new game -> Project/<game-name>/ following Project/README.md.
  GAME TYPE GENERATION: template/docs/game_type_generation.md is the entry point.
  76 archetypes x 6 layout patterns x 26 themes via
  python template/tools/worldgen/gen_archetype.py --archetype <key> --theme <t>
  (flagships: gen_soulslike/gen_space/gen_tabletop/gen_platformer25d).
  Lookup "like <game>" in template/tools/worldgen/genre_index.csv.
  Make playable: ENGINE.bat/.sh (Vulkan player) ships with every game;
  litt view <game> opens the native C++ viewer. No HTML anywhere.
  One-phrase multi-region world: litt forge "<phrase>" -> WorldForge
  litt.worldforge/1 spec -> fused game; refine loop: litt refine [...].
- engine bug/feature -> say so first, read docs/ARCHITECTURE.md before any code edit.
- pure question -> answer only, write nothing anywhere.

Orient before write. Record every seed. Register every asset (asset_index.json +
ATTRIBUTION.md). Determinism: same seed = same bytes; state written LAST.
Full protocol: AGENTS.md | Algorithms as code:
template/tools/worldgen/algokit.py | Math cookbook:
template/docs/procedural_asset_math.md
