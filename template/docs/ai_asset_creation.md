# AI Asset Creation Guide

How an AI agent creates game assets (models, textures, environments) for Litt
Engine - without art tools. Read this top to bottom before generating anything.

Related: [asset_guidelines.md](asset_guidelines.md) (size budgets) |
[browser_asset_ingest.md](browser_asset_ingest.md) (downloaded assets)

---

## Ground Rules (non-negotiable)

1. **Units:** meters. **Axes:** Y-up, right-handed. **Winding:** counter-clockwise
   front faces. **Origin:** model base center (feet at y=0).
2. **Budgets** (see asset_guidelines.md): model < 500 KB, texture < 256 KB,
   total project < 5 MB.
3. **Every asset must be registered**: entry in `assets/asset_index.json` AND a
   provenance row in `template/assets/ATTRIBUTION.md`. Unregistered assets are
   invisible to other agents and will be treated as orphaned junk.
4. **License discipline:** generated-in-repo = cleanest. Downloads need CC0 /
   CC-BY / MIT / Apache-2.0 (see ingest doc). Never commit assets of unclear origin.

---

## Choose Your Route

```
What do you need?
|- building / crate / fence / terrain / simple prop ....... ROUTE 1 (procedural)
|- organic unique hero prop (statue, sword, creature) ..... ROUTE 2 (image-to-3D)
|- whole pack of polished standard pieces ................. ROUTE 3 (CC0 download)
|- placing assets into a world ........................... SCENE ASSEMBLY (below)
```

Default to Route 1. Escalate only when math genuinely cannot describe the shape.

---

## Route 1: Procedural Generation (preferred)

A 3D model is just text: `v` lines are vertex positions, `f` lines connect them
into triangles, `.mtl` sidecar defines flat colors. No sculpting required -
complexity comes from *composing primitives with parameters*.

**Every vertex/face table, noise function and placement algorithm is written
out in [procedural_asset_math.md](procedural_asset_math.md)** - read it once,
then use the working reference implementation:

```bash
python template/tools/procedural_assets.py house   --name village_house_01
python template/tools/procedural_assets.py cottage --name old_cottage
python template/tools/procedural_assets.py tree    --name pine_tree_01
python template/tools/procedural_assets.py crate   --name barrel_crate
```

Each run: writes `.obj` + `.mtl`, validates the size budget, prints SHA-256,
and auto-registers the asset in `asset_index.json`.

> **Where outputs go:** game content ALWAYS lives under `Project/<game-name>/assets`
> (pass `--out-dir Project/<game-name>/assets`). The engine-root `assets/` folder is
> reserved for engine examples. See `Project/README.md`.

To extend it, read the script once (~200 lines, stdlib only). The core pattern:

| Primitive | Composes into |
|-----------|---------------|
| `box()` | buildings, crates, fences, tables, chimneys |
| `roof_prism()` | gable roofs |
| `pyramid()` | tree canopies, spires, tent roofs |
| noise heightmap + grid mesh | terrain |

A village = one house function called 50 times with varied parameters.
That is not a compromise - it is how real low-poly games ship.

### Style guidance
- Low-poly is a legitimate art direction (Crossy Road, Superhot).
- Keep triangle counts in the hundreds, not millions.
- One MTL file per model, 3-5 flat materials max.
- Consistent palette across all assets of a game (pick 8-12 hex colors first,
  reuse everywhere).

---

## Route 2: Image-to-3D Chain (hero props only)

Stable Diffusion makes images, never meshes. For a unique organic prop:

1. Generate reference image(s): local SD (ComfyUI :8188 / A1111 :7860 API) or cloud API.
2. Reconstruct to mesh: TripoSR (local, free) or Meshy/Tripo/Rodin (API).
   Output arrives as `.glb` - already the engine's preferred format.
3. Decimate to fit budget: `npx gltf-transform optimize in.glb out.glb --texture-compress ktx2`.
4. Register + attribute (model name goes in ATTRIBUTION.md).

Weaknesses: messy topology, blobby hard-surfaces. Do NOT use this for buildings
or terrain - Route 1 wins there on every axis.

Animated humanoids: skip straight to Mixamo (free auto-rigged characters +
animations, exports FBX/GLB).

---

## Route 3: Downloading CC0 Packs

Best quality-per-minute when starting a game. Sources: Kenney.nl, Quaternius,
Poly Haven, OpenGameArt (filter by license).

Follow [browser_asset_ingest.md](browser_asset_ingest.md): validate license,
copy to `assets/models/`, register, attribute. Modular kits snap together on a
grid - prefer kits over single mega-models.

---

## Scene Assembly (the environment itself)

An environment is NEVER one giant mesh. It is data:

```
kit pieces + placement script -> assets/scenes/world.lscn.json
```

Placement logic an agent computes (never hand-typed):
- Buildings along roads / grid cells, door facing road
- Trees/rocks scattered where value-noise > threshold, minimum-spacing enforced
- Collision volumes match visual bounds (box/capsule approximations are fine)

---

## Registration Checklist (every asset, every time)

1. File lands in `assets/models|	extures|audio/` with kebab-case name.
2. Size within budget (`Get-Item <file>.Length` or equivalent).
3. `assets/asset_index.json`: unique id, type, relative path, loader string.
4. `template/assets/ATTRIBUTION.md`: method/source + license row.
5. Index still parses: `ConvertFrom-Json` (pwsh) or `json.load` (python).
6. Log the action to `template/agent/actions.log`.

If any step fails: fix before moving on. An unregistered or oversized asset is
a bug, not a style choice.
