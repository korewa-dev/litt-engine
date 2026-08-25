#!/usr/bin/env python3
"""world_forge.py - WorldForge composer (CDR-011 item 2).

Consumes a litt.worldforge/1 spec (docs/specs/WORLDFORGE_SPEC.md) and fuses
multiple generated regions into ONE explorable game:

  python world_forge.py <spec.json> [--out-dir Project/<name>] [--force]
                        [--skip-native-proof]

Pipeline:
  1. Load + defensively validate the spec (same V-rules as
     world_planner.py --spec-in; unknown keys rejected).
  2. Per region k (array order): invoke make_game.py --kind <generator>
     --name "<name>-<id>" --seed <seed+k> --out-dir %TEMP%/wf-<name>/<id>
     (--archetype/--pattern/--theme forwarded when present; theme always;
     inner builds skip the redundant per-region pixel proof).
  3. FUSE into one game dir named spec.name:
       - assets/models merged under per-region prefixes (<rid>__<orig>);
         OBJ mtllib lines rewritten to each region's namespaced MTLs;
       - asset_index entries merged with provenance.region recorded;
       - scene nodes merged: origin offset onto every position
         (x += origin[0], z += origin[2]), names prefixed <rid>__, tags
         intact except model: refs which follow the rename;
       - world_state identity/gameplay kept from the SPAWN region;
         objective composed from objective_chain_hint;
       - LINKS: two portal gate nodes (tag ["goal","portal"]) per linked
         pair at the midpoint between origins +/- 4 m, reusing a kit
         goal_gate mesh when any region shipped one, else emitting one
         shared box-arch OBJ;
       - SPAWN: exactly one player/start node survives (the spawn
         region's); duplicates from other regions are stripped + counted.
  4. GATES: lint_game clean -> littcli validate --frames 120 ok:true AND
     interactives > 0 AND missing == 0 -> native_proof.proof_one_game PASS
     (fill >= 1.5%, colors >= 8) unless --skip-native-proof.
     ENGINE.bat/.sh + VIEW.bat copied verbatim from a region scratch
     (generic launchers); NOTES.md gets the region table + seeds.
  5. Output: last stdout line is machine-readable JSON:
       {"ok":true,"game":dir,"regions":{...},"portals":p,"native_proof":{}}

Deterministic: same spec -> byte-identical assets/scenes/world.lscn.json.
Stdlib only; reads sibling tools (lint.py, native_proof.py, make_game.py,
worldkit.py, themes.json, design_rules.json); writes only --out-dir and its
%TEMP% scratch.
"""
import argparse
import datetime
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ASSETS_TOOLS = HERE.parent / "assets"
REPO = HERE.parents[2]
sys.path.insert(0, str(ASSETS_TOOLS))
sys.path.insert(0, str(HERE))
from lint import lint_game  # noqa: E402
from native_proof import proof_one_game  # noqa: E402

SCHEMA = "litt.worldforge/1"
GENERATORS = ("soulslike", "space", "tabletop", "platformer25d", "archetype")
ROLES = ("start", "middle", "finale")
PATTERNS = ("arena_ring", "corridor_run", "hub_spoke",
            "grid_board", "spline_track", "room_graph")
NAME_RE = re.compile(r"^[a-z0-9][a-z0-9-]{0,47}$")
ID_RE = re.compile(r"^[a-z0-9][a-z0-9_-]{0,63}$")
TOP_KEYS = {"schema", "name", "about", "seed", "regions",
            "spawn_region", "objective_chain_hint"}
REGION_KEYS = {"id", "generator", "archetype", "pattern", "theme",
               "role", "origin", "links", "size"}
SIZE_MIN, SIZE_MAX = 24, 140

PORTAL_OFFSET_M = 4.0
MIN_FILL_PCT = 1.5
MIN_COLORS = 8
NL = chr(10)


class SpecError(ValueError):
    """Spec failed defensive validation (every violation listed)."""


class ForgeError(RuntimeError):
    """A forge stage failed (region build, gate, IO)."""


# --------------------------------------------------------------- spec layer
def load_themes():
    """Theme keys available in themes.json (planner uses the same file)."""
    data = json.loads((HERE / "themes.json").read_text(encoding="utf-8"))
    return set(data.get("themes", {}).keys())


def load_archetypes():
    """Archetype keys from design_rules.json (V017 vocabulary)."""
    rules = json.loads((HERE / "design_rules.json").read_text(
        encoding="utf-8"))
    return set(rules.get("archetypes", {}).keys())


def _is_num(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def validate_spec(raw, themes=None, archetypes=None):
    """Defensively validate a parsed litt.worldforge/1 document.

    Returns a normalized deep copy; raises SpecError listing EVERY problem.
    Violation codes mirror world_planner.py --spec-in (V001..V026) so hand
    edits get consistent errors from either tool. One documented divergence:
    an archetype-generator region WITHOUT pattern is accepted here (V018 is
    satisfied by composer-side derivation per spec section 4) instead of
    being rejected - docs/specs/WORLDFORGE_SPEC.md delegates that derivation
    to the composer."""
    V = []

    def bad(code, where, detail):
        V.append("V%s %s: %s" % (code, where, detail))

    def want(cond, code, where, detail):
        if not cond:
            bad(code, where, detail)
        return cond

    if not want(isinstance(raw, dict), "001", "$",
                "spec must be a JSON object"):
        raise SpecError("spec rejected:%s%s" % (NL, NL.join(V)))
    for key in sorted(set(raw.keys()) - TOP_KEYS):
        bad("026", "$", "unknown top-level key %r" % key)

    want(raw.get("schema") == SCHEMA, "002", "$.schema",
         'must be "%s", got %r' % (SCHEMA, raw.get("schema")))
    name = raw.get("name")
    if not want(isinstance(name, str) and NAME_RE.match(name or ""),
                "003", "$.name",
                "must match ^[a-z0-9][a-z0-9-]{0,47}$, got %r" % (name,)):
        name = None
    about = raw.get("about")
    want(isinstance(about, str) and about.strip(), "004", "$.about",
         "must be a non-empty string")
    seed = raw.get("seed")
    if not want(not isinstance(seed, bool) and isinstance(seed, int)
                and seed >= 0, "005", "$.seed",
                "must be an int >= 0, got %r" % (seed,)):
        seed = 0

    regions = raw.get("regions")
    if not want(isinstance(regions, list) and 2 <= len(regions) <= 5,
                "006", "$.regions",
                "need 2..5 regions, got %r"
                % (len(regions) if isinstance(regions, list) else regions)):
        raise SpecError("spec rejected:%s%s" % (NL, NL.join(V)))

    norm_regions = []
    ids, starts, finales = [], [], []
    for i, reg in enumerate(regions):
        where = "$.regions[%d]" % i
        if not want(isinstance(reg, dict), "001", where,
                    "region must be an object"):
            continue
        for key in sorted(set(reg.keys()) - REGION_KEYS):
            bad("026", where, "unknown region key %r" % key)

        rid = reg.get("id")
        if not want(isinstance(rid, str) and ID_RE.match(rid or ""),
                    "010", where + ".id", "bad id %r" % (rid,)):
            rid = None
        elif rid in ids:
            bad("010", where + ".id", "duplicate id %r" % rid)
        else:
            ids.append(rid)

        gen = reg.get("generator")
        want(gen in GENERATORS, "011", where + ".generator",
             "must be one of %s, got %r" % ("|".join(GENERATORS), gen))

        theme = reg.get("theme")
        if isinstance(theme, str) and themes is not None:
            want(theme in themes, "012", where + ".theme",
                 "unknown theme %r (see themes.json)" % (theme,))
        elif not isinstance(theme, str):
            bad("012", where + ".theme",
                "must be a themes.json key, got %r" % (theme,))

        role = reg.get("role")
        if role not in ROLES:
            bad("013", where + ".role",
                "must be start|middle|finale, got %r" % (role,))
        elif role == "start":
            starts.append(i)
        elif role == "finale":
            finales.append(i)

        origin = reg.get("origin")
        ok_origin = (isinstance(origin, list) and len(origin) == 3
                     and all(_is_num(c) for c in origin))
        if not want(ok_origin, "014", where + ".origin",
                    "must be [x, y, z] numbers, got %r" % (origin,)):
            origin = [0, 0, 0]

        size = reg.get("size")
        good_size = (isinstance(size, int) and not isinstance(size, bool)
                     and SIZE_MIN <= size <= SIZE_MAX)
        if not want(good_size, "015", where + ".size",
                    "must be int in %d..%d, got %r"
                    % (SIZE_MIN, SIZE_MAX, size)):
            size = None

        links = reg.get("links")
        if not want(isinstance(links, list)
                    and all(isinstance(x, str) for x in links),
                    "016", where + ".links", "must be a list of region ids"):
            links = []
        else:
            if rid in links:
                bad("016", where + ".links", "self-link on %r" % rid)
            if len(set(links)) != len(links):
                bad("016", where + ".links", "duplicate link targets")
            all_ids = [r.get("id") for r in regions if isinstance(r, dict)]
            for tgt in links:
                if tgt not in all_ids:
                    bad("016", where + ".links",
                        "missing link target %r" % tgt)

        arch = reg.get("archetype", None)
        if gen == "archetype":
            if isinstance(arch, str) and archetypes is not None:
                want(arch in archetypes, "017", where + ".archetype",
                     "required for generator=archetype and must exist in "
                     "design_rules.json, got %r" % (arch,))
            elif not isinstance(arch, str):
                bad("017", where + ".archetype",
                    "required for generator=archetype, got %r" % (arch,))
        elif arch is not None:
            bad("017", where + ".archetype",
                "only allowed when generator=archetype, got %r" % (arch,))

        pat = reg.get("pattern", None)
        if gen == "archetype":
            # Composer divergence: None here is legal - derive_pattern()
            # supplies one from the archetype's structure (spec section 4).
            want(pat is None or pat in PATTERNS, "018", where + ".pattern",
                 "must be one of %s when present, got %r"
                 % ("|".join(PATTERNS), pat))
        elif pat is not None:
            bad("018", where + ".pattern",
                "only allowed when generator=archetype, got %r" % (pat,))

        norm_regions.append({
            "id": rid or "", "generator": gen, "archetype": arch,
            "pattern": pat, "theme": theme, "role": role,
            "origin": [float(origin[0]), float(origin[1]),
                       float(origin[2])],
            "links": list(links), "size": size})

    want(len(starts) == 1, "020", "$.regions",
         "exactly one role=start required, found %d" % len(starts))
    want(len(finales) == 1, "021", "$.regions",
         "exactly one role=finale required, found %d" % len(finales))

    spawn = raw.get("spawn_region")
    start_id = regions[starts[0]].get("id") if starts else None
    if starts and spawn != start_id:
        bad("022", "$.spawn_region",
            "must equal the id of the start region %r, got %r"
            % (start_id, spawn))

    hint = raw.get("objective_chain_hint")
    if not want(isinstance(hint, list) and len(hint) == len(regions)
                and all(isinstance(s, str) and s.strip() for s in hint),
                "023", "$.objective_chain_hint",
                "need one non-empty string per region (%d), got %r"
                % (len(regions), hint)):
        hint = None

    # V024 spacing: every pairwise origin distance >= half-sum of sizes.
    good = [(r["origin"], r["size"]) for r in norm_regions
            if r["size"] is not None]
    for ai in range(len(good)):
        for bi in range(ai + 1, len(good)):
            oa, sa = good[ai]
            ob, sb = good[bi]
            d = math.sqrt(sum((oa[c] - ob[c]) ** 2 for c in range(3)))
            need = (sa + sb) / 2.0
            if d < need:
                bad("024", "$.regions origins",
                    "%.1f apart but sizes need >= %.1f" % (d, need))

    # V025 directed reachability from spawn_region over links.
    if starts and spawn == start_id:
        adj = {r["id"]: r["links"] for r in norm_regions}
        seen, queue = set(), [spawn]
        while queue:
            cur = queue.pop()
            if cur in seen:
                continue
            seen.add(cur)
            queue.extend(t for t in adj.get(cur, []) if t not in seen)
        unreachable = [i for i in ids if i and i not in seen]
        if unreachable:
            bad("025", "$.links",
                "unreachable from spawn_region: %s" % ", ".join(unreachable))

    if V:
        raise SpecError("spec rejected:%s%s" % (NL, NL.join(V)))
    return {"schema": SCHEMA, "name": name, "about": about, "seed": seed,
            "regions": norm_regions, "spawn_region": spawn,
            "objective_chain_hint": hint}


def load_spec(path):
    raw = json.loads(Path(path).read_text(encoding="utf-8"))
    return validate_spec(raw, load_themes(), load_archetypes())


def spec_by_id(spec):
    return {r["id"]: r for r in spec["regions"]}


def region_seed(spec, rid):
    """Deterministic per-region seed: spec.seed + index in array order."""
    k = [r["id"] for r in spec["regions"]].index(rid)
    return spec["seed"] + k


STRUCTURE_HINTS = (                                    # structure substrings
    (("procedural", "dungeon"), "room_graph"),
    (("hub_spoke", "semi_open", "metroidvania"), "hub_spoke"),
    (("mission_based", "arenas", "wave_based"), "arena_ring"),
    (("infinite", "linear"), "corridor_run"),
)


def derive_pattern(reg):
    """Pattern for an archetype region that omits one (spec section 4):
    mapped from design_rules' structure field, default arena_ring."""
    if reg.get("pattern"):
        return reg["pattern"]
    try:
        rules = json.loads((HERE / "design_rules.json").read_text(
            encoding="utf-8"))
        structure = rules["archetypes"].get(reg["archetype"], {}).get(
            "structure") or ""
    except Exception:
        structure = ""
    for keys, pat in STRUCTURE_HINTS:
        if any(k in structure for k in keys):
            return pat
    return "arena_ring"


def objective_text(spec):
    """Composed gameplay objective from the chain hints."""
    hints = spec.get("objective_chain_hint") or []
    if hints:
        return " ".join(h.strip() for h in hints)
    ids = " -> ".join(r["id"] for r in spec["regions"])
    return "journey across %d regions: %s" % (len(spec["regions"]), ids)


# -------------------------------------------------------------- math helpers
def offset_pos(pos, origin):
    """Region-local node position -> fused-world position.

    Origin offsets apply to x and z only; y stays region-local ground."""
    return [round(float(pos[0]) + float(origin[0]), 4),
            round(float(pos[1]), 4),
            round(float(pos[2]) + float(origin[2]), 4)]


def prefixed(region_id, base):
    """Namespaced asset/node identifier: <region_id>__<base>."""
    return "%s__%s" % (region_id, base)


def yaw_quat(yaw_deg):
    """Y-only rotation quaternion, worldkit.write_scene convention."""
    rad = math.radians(yaw_deg)
    return [0.0, round(math.sin(rad / 2.0), 4), 0.0,
            round(math.cos(rad / 2.0), 4)]


def plan_links(spec):
    """Directed links -> unique unordered pairs, first-seen order."""
    pairs, seen = [], set()
    for r in spec["regions"]:
        for other in r["links"]:
            key = tuple(sorted((r["id"], other)))
            if key not in seen:
                seen.add(key)
                pairs.append((r["id"], other))
    return pairs


def portal_placement(oa, ob, offset=PORTAL_OFFSET_M):
    """Two facing gate positions + yaws for one linked pair.

    Gates straddle the midpoint between the origins +/- `offset` meters
    along the origin-to-origin axis, turned to face each other."""
    dx = float(ob[0]) - float(oa[0])
    dz = float(ob[2]) - float(oa[2])
    length = math.hypot(dx, dz)
    if length < 1e-9:
        ux, uz = 1.0, 0.0
    else:
        ux, uz = dx / length, dz / length
    mx = (float(oa[0]) + float(ob[0])) / 2.0
    my = (float(oa[1]) + float(ob[1])) / 2.0
    mz = (float(oa[2]) + float(ob[2])) / 2.0
    pa = [round(mx - ux * offset, 4), round(my, 4),
          round(mz - uz * offset, 4)]
    pb = [round(mx + ux * offset, 4), round(my, 4),
          round(mz + uz * offset, 4)]
    yaw_ab = round(math.degrees(math.atan2(dx, dz)), 4)
    yaw_ba = round(math.degrees(math.atan2(-dx, -dz)), 4)
    return pa, pb, yaw_ab, yaw_ba


# ------------------------------------------------------------ region builds
def scratch_root(spec):
    safe = "".join(c if c.isalnum() or c in "-_" else "_" for c
                   in (spec["name"] or "spec"))
    return Path(tempfile.gettempdir()) / ("wf-%s" % safe)


def make_game_cmd(spec, reg, out_dir, seed):
    cmd = [sys.executable, str(HERE / "make_game.py"),
           "--kind", reg["generator"],
           "--name", "%s-%s" % (spec["name"], reg["id"]),
           "--seed", str(seed),
           "--out-dir", str(out_dir)]
    if reg["generator"] == "archetype":
        cmd += ["--archetype", reg["archetype"],
                "--pattern", derive_pattern(reg)]
    cmd += ["--theme", reg["theme"], "--about", spec["about"],
            # Region-level pixel proofs are redundant: the FUSED world runs
            # the full validate+proof gates below.
            "--skip-native-proof"]
    return cmd


def build_region(spec, reg, out_dir, seed):
    """Run make_game.py into a fresh namespaced scratch dir."""
    out_dir = Path(out_dir)
    if out_dir.exists():
        shutil.rmtree(out_dir)
    print("[forge] region %-20s kind=%-13s seed=%d -> %s"
          % (reg["id"], reg["generator"], seed, out_dir))
    proc = subprocess.run(make_game_cmd(spec, reg, out_dir, seed),
                          capture_output=True, text=True,
                          env=dict(os.environ, LITT_NO_MANIFEST="1"))
    tail = ""
    for ln in reversed(proc.stdout.strip().splitlines()):
        if ln.startswith("{"):
            tail = ln
            break
    if proc.returncode != 0:
        raise ForgeError("make_game failed for region %r (%s)%s%s%s%s"
                         % (reg["id"], tail[-200:], NL, proc.stdout[-600:],
                            NL, proc.stderr[-600:]))
    scene = out_dir / "assets" / "scenes" / "world.lscn.json"
    state = out_dir / "world_state.json"
    if not scene.exists() or not state.exists():
        raise ForgeError("region %r produced incomplete output at %s"
                         % (reg["id"], out_dir))
    print("[forge] region %-20s ok %s" % (reg["id"], tail))


# ------------------------------------------------------------------ merging
def merge_models(src_game, out_dir, rid):
    """Copy a region's OBJs/MTLs under <rid>__ prefixes, rewriting each
    OBJ's mtllib line to its namespaced MTL copy. Returns original stems."""
    src = Path(src_game) / "assets" / "models"
    dst = out_dir / "assets" / "models"
    dst.mkdir(parents=True, exist_ok=True)
    mtl_map = {}
    for mtl in sorted(src.glob("*.mtl")):
        new_name = prefixed(rid, mtl.name)
        shutil.copyfile(mtl, dst / new_name)
        mtl_map[mtl.name] = new_name
    stems = []
    for obj in sorted(src.glob("*.obj")):
        fixed = []
        for ln in obj.read_text(encoding="utf-8").splitlines():
            if ln.startswith("mtllib "):
                ref = ln[7:].strip()
                ln = "mtllib " + mtl_map.get(ref, ref)
            fixed.append(ln)
        (dst / prefixed(rid, obj.name)).write_text(
            NL.join(fixed) + NL, encoding="utf-8")
        stems.append(obj.stem)
    return stems


def merge_asset_index(src_game, out_dir, rid, extra_entries=()):
    """Append a region's asset_index entries under prefixed ids with
    provenance.region recorded. Entries whose renamed file is absent are
    skipped defensively."""
    idx_path = Path(src_game) / "assets" / "asset_index.json"
    try:
        data = json.loads(idx_path.read_text(encoding="utf-8"))
    except Exception:
        data = {}
    out_path = out_dir / "assets" / "asset_index.json"
    if out_path.exists():
        index = json.loads(out_path.read_text(encoding="utf-8"))
    else:
        index = {"format": "litt-asset-index", "version": 1,
                 "description": "Merged by world_forge.py (CDR-011).",
                 "assets": []}
    assets = index.setdefault("assets", [])
    have_ids = {entry.get("id") for entry in assets}
    added = 0
    for e in data.get("assets", []):
        if not isinstance(e, dict) or "id" not in e:
            continue
        parts = str(e.get("path", "")).replace("\\\\", "/").split("/")
        parts[-1] = prefixed(rid, parts[-1])
        new_path = "/".join(parts)
        if not (out_dir / "assets" / new_path).exists():
            continue
        ne = {"id": prefixed(rid, e["id"]),
              "type": e.get("type", "model"),
              "path": new_path,
              "loader": e.get("loader",
                              "litt_asset::manager::AssetManager"
                              "::load_model"),
              "provenance": {"region": rid}}
        if ne["id"] not in have_ids:
            assets.append(ne)
            have_ids.add(ne["id"])
            added += 1
    for e in extra_entries:
        if e["id"] not in have_ids:
            assets.append(e)
            have_ids.add(e["id"])
            added += 1
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(index, indent=2) + NL, encoding="utf-8")
    return added


def load_scene(game_dir):
    p = Path(game_dir) / "assets" / "scenes" / "world.lscn.json"
    return json.loads(p.read_text(encoding="utf-8"))


def new_root(children):
    return {"name": "Root", "id": 0, "parent": None, "children": children,
            "position": [0, 0, 0], "rotation": [0, 0, 0, 1],
            "scale": [1, 1, 1], "visible": True, "layer": 0, "tags": []}


def convert_node(node, rid, origin, new_id):
    """Namespace + offset one region node into fused-scene form.

    Name gets the <rid>__ prefix, position the region origin offset (x/z);
    tags stay intact except model: refs which follow the asset rename.
    Nodes are re-rooted flat under Root."""
    tags_in = node.get("tags")
    if not isinstance(tags_in, list):
        tags_in = []
    tags = [("model:" + prefixed(rid, t[6:]))
            if isinstance(t, str) and t.startswith("model:") else t
            for t in tags_in]
    pos = node.get("position", [0, 0, 0])
    if not (isinstance(pos, list) and len(pos) == 3):
        pos = [0, 0, 0]
    rot = node.get("rotation", [0, 0, 0, 1])
    scale = node.get("scale", [1, 1, 1])
    return {"name": prefixed(rid, node.get("name", "node")),
            "id": new_id, "parent": 0, "children": [],
            "position": offset_pos(pos, origin),
            "rotation": rot, "scale": scale,
            "visible": node.get("visible", True),
            "layer": node.get("layer", 0), "tags": tags}


def is_player_start(node):
    tags = node.get("tags")
    if not isinstance(tags, list):
        return False
    tset = set(tags)
    return "player" in tset and "start" in tset


def emit_portal_arch(out_dir):
    """Shared box-arch portal OBJ emitted ONCE (fallback when no region
    shipped a kit goal_gate mesh). Origin-centered (double-transform safe),
    two materials, ~3.5 m tall: pillars + lintel + emissive threshold."""
    from worldkit import MeshBuilder
    mb = MeshBuilder()
    mb.begin("wf_pillar", "portal_frame")
    mb.box(-1.2, 0.0, -0.25, 0.35, 3.0, 0.5)
    mb.box(0.85, 0.0, -0.25, 0.35, 3.0, 0.5)
    mb.begin("wf_lintel", "portal_frame")
    mb.box(-1.55, 3.0, -0.25, 3.1, 0.45, 0.5)
    mb.begin("wf_threshold", "portal_glow")
    mb.box(-1.2, 0.0, -0.05, 2.4, 0.12, 0.1)
    models = out_dir / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    (models / "wf_portal.mtl").write_text(NL.join([
        "newmtl portal_frame",
        "Ka 1.000 1.000 1.000", "Kd 0.450 0.400 0.550",
        "Ks 0.050 0.050 0.050", "Ns 8.0", "",
        "newmtl portal_glow",
        "Ka 1.000 1.000 1.000", "Kd 0.950 0.850 0.300",
        "Ke 0.900 0.750 0.250", "Ks 0.050 0.050 0.050", "Ns 8.0", ""]),
        encoding="utf-8")
    obj_text, _, _ = mb.to_obj("wf_goal_gate", "wf_portal")
    (models / "wf_goal_gate.obj").write_text(obj_text, encoding="utf-8")


def portal_ref(stems_by_region):
    """Reuse a kit goal_gate mesh when any region shipped one (first in
    spec order wins); caller emits the shared arch otherwise. Returns
    (model_ref, emit_arch_needed)."""
    for rid in stems_by_region:
        if "goal_gate" in stems_by_region[rid]:
            return prefixed(rid, "goal_gate"), False
    return "wf_goal_gate", True


def fuse_scene(spec, scratches, out_dir, stems_by_region):
    """Merge every region's nodes into one scene, place portal gates on the
    links, guarantee exactly one player/start. Writes
    assets/scenes/world.lscn.json. Returns (per-region node counts, portal
    gate count, stripped duplicate spawn count)."""
    spawn_rid = spec["spawn_region"]
    nodes = []
    per_region = {}
    stripped = 0
    for reg in spec["regions"]:
        rid = reg["id"]
        scene = load_scene(scratches[rid])
        root_id = scene.get("root_id", 0)
        count = 0
        for node in scene.get("nodes", []):
            if not isinstance(node, dict):
                continue
            if node.get("id") == root_id or node.get("parent") is None:
                continue                       # region Root stays behind
            if is_player_start(node):
                if rid != spawn_rid:
                    stripped += 1              # duplicate spawn: dropped
                    continue
            nodes.append(convert_node(node, rid, reg["origin"],
                                      len(nodes) + 1))
            count += 1
        per_region[rid] = count

    ref, need_arch = portal_ref(stems_by_region)
    if need_arch:
        emit_portal_arch(out_dir)
    portals = 0
    by_id = spec_by_id(spec)
    for a, b in plan_links(spec):
        pa, pb, ya, yb = portal_placement(by_id[a]["origin"],
                                          by_id[b]["origin"])
        for owner, peer, pos, yaw in ((a, b, pa, ya), (b, a, pb, yb)):
            nodes.append({
                "name": "Portal_%s__to_%s" % (owner, peer),
                "id": len(nodes) + 1, "parent": 0, "children": [],
                "position": pos, "rotation": yaw_quat(yaw),
                "scale": [1, 1, 1], "visible": True, "layer": 0,
                "tags": ["goal", "portal", "model:" + ref]})
        portals += 2

    children = list(range(1, len(nodes) + 1))
    fused = {"format": "litt-scene", "version": 1, "root_id": 0,
             "next_id": len(nodes) + 1,
             "nodes": [new_root(children)] + nodes}
    spath = out_dir / "assets" / "scenes" / "world.lscn.json"
    spath.parent.mkdir(parents=True, exist_ok=True)
    spath.write_text(json.dumps(fused, indent=2) + NL, encoding="utf-8")
    return per_region, portals, stripped


def fuse_state(spec, scratches, out_dir, portals):
    """Fused world_state = spawn region's identity/gameplay VERBATIM, with
    the composed objective, the spawn point shifted into fused coordinates,
    and a worldforge meta block appended."""
    reg = spec_by_id(spec)[spec["spawn_region"]]
    st = json.loads((Path(scratches[spec["spawn_region"]])
                     / "world_state.json").read_text(encoding="utf-8"))
    gp = st.get("gameplay") if isinstance(st.get("gameplay"), dict) else {}
    if isinstance(gp.get("spawn"), list) and len(gp["spawn"]) == 3:
        gp["spawn"] = offset_pos(gp["spawn"], reg["origin"])
    gp["objective"] = objective_text(spec)
    st["gameplay"] = gp
    seed_block = st.get("seed") if isinstance(st.get("seed"), dict) else {}
    seed_block["worldforge_seed"] = spec["seed"]
    st["seed"] = seed_block
    meta = st.get("meta") if isinstance(st.get("meta"), dict) else {}
    meta["worldforge"] = {
        "schema": SCHEMA, "spawn_region": spec["spawn_region"],
        "regions": [{"id": r["id"], "generator": r["generator"],
                     "role": r["role"], "origin": r["origin"],
                     "theme": r["theme"], "seed": region_seed(spec, r["id"])}
                    for r in spec["regions"]],
        "portals": portals}
    st["meta"] = meta
    (out_dir / "world_state.json").write_text(
        json.dumps(st, indent=2) + NL, encoding="utf-8")


def fuse_spec_into_game(spec, scratches, out_dir):
    """Merge pre-built region scratch dirs into `out_dir` (pure merge, no
    subprocesses - also the unit-test entry point). Returns stats dict."""
    out_dir = Path(out_dir)
    if out_dir.exists():
        shutil.rmtree(out_dir)
    (out_dir / "assets" / "models").mkdir(parents=True)
    (out_dir / "assets" / "scenes").mkdir(parents=True)

    stems_by_region, obj_counts = {}, {}
    for reg in spec["regions"]:
        rid = reg["id"]
        stems_by_region[rid] = merge_models(scratches[rid], out_dir, rid)
        obj_counts[rid] = len(stems_by_region[rid])

    per_nodes, portals, stripped = fuse_scene(spec, scratches, out_dir,
                                              stems_by_region)
    fuse_state(spec, scratches, out_dir, portals)

    forge_entry = None
    if (out_dir / "assets" / "models" / "wf_goal_gate.obj").exists():
        forge_entry = {"id": "wf_goal_gate", "type": "model",
                       "path": "models/wf_goal_gate.obj",
                       "loader": "litt_asset::manager::AssetManager"
                                 "::load_model",
                       "provenance": {"region": "__worldforge__"}}
    for reg in spec["regions"]:
        merge_asset_index(scratches[reg["id"]], out_dir, reg["id"],
                          [forge_entry] if forge_entry else ())

    stats = {}
    for reg in spec["regions"]:
        rid = reg["id"]
        stats[rid] = {"nodes": per_nodes.get(rid, 0),
                      "objs": obj_counts.get(rid, 0),
                      "seed": region_seed(spec, rid),
                      "origin": reg["origin"]}
    return {"regions": stats, "portals": portals,
            "stripped_spawn_duplicates": stripped}


# ------------------------------------------------------------------- gates
def find_bin(name):
    for cand in (REPO / "native" / "bin" / (name + ".exe"),
                 REPO / "native" / "bin" / name):
        if cand.exists():
            return cand
    return None


def run_gates(out_dir, skip_native_proof):
    """lint -> littcli validate --frames 120 -> native proof.
    Returns (report, sim_json, proof_or_None); raises ForgeError naming
    every failed assertion."""
    report = lint_game(out_dir)
    problems = list(report["problems"]) + [
        "dangling model ref: %s" % ref for ref in report["dangling_refs"]]
    if problems:
        raise ForgeError("lint failed:%s%s" % (
            NL, "".join("  - %s%s" % (p, NL) for p in problems[:20])))
    print("[forge] lint: clean (%d objs)" % report["objs"])

    cli = find_bin("littcli")
    if cli is None:
        raise ForgeError("native/bin/littcli not built")
    proc = subprocess.run([str(cli), "validate", str(out_dir),
                           "--frames", "120"],
                          capture_output=True, text=True, timeout=120)
    print(proc.stdout.strip())
    js = {}
    for ln in reversed(proc.stdout.strip().splitlines()):
        if ln.startswith("{"):
            try:
                js = json.loads(ln)
            except Exception:
                js = {}
            break
    gate_problems = []
    if proc.returncode != 0 or not js.get("ok"):
        gate_problems.append("validate ok:true (rc=%s ok=%r)"
                             % (proc.returncode, js.get("ok")))
    inter = js.get("interactives")
    if not isinstance(inter, int) or inter <= 0:
        gate_problems.append("interactives > 0 (got %r)" % (inter,))
    if js.get("missing") != 0:
        gate_problems.append("missing == 0 (got %r)" % (js.get("missing"),))

    proof = None
    if skip_native_proof:
        print("[forge] native-proof: SKIPPED (--skip-native-proof)")
    elif gate_problems:
        pass                    # sim broken: proof would double-report it
    else:
        view = find_bin("littview")
        if view is None:
            gate_problems.append("native/bin/littview present")
        else:
            rec = proof_one_game(out_dir, cli, view, MIN_FILL_PCT,
                                 MIN_COLORS, sim=js)
            proof = {"verdict": rec.get("verdict"),
                     "fill": round(rec.get("fill_pct", 0.0), 2),
                     "colors": rec.get("colors"),
                     "interactives": rec.get("interactives"),
                     "missing": rec.get("missing")}
            if rec.get("verdict") != "PASS":
                gate_problems.extend(rec.get("problems", [])
                                     or ["proof verdict FAIL"])
    if gate_problems:
        raise ForgeError("gates failed:%s%s" % (
            NL, "".join("  - %s%s" % (p, NL) for p in gate_problems)))
    if proof:
        print("[forge] native-proof: PASS | fill=%.2f%% colors=%s "
              "interactives=%s missing=%s"
              % (proof["fill"], proof["colors"], proof["interactives"],
                 proof["missing"]))
    return report, js, proof


def deploy_launchers(scratch, out_dir):
    """ENGINE.bat/.sh + VIEW.bat are generic relative-path launchers -
    copied verbatim from any region's scratch build."""
    copied = []
    for name in ("ENGINE.bat", "ENGINE.sh", "VIEW.bat"):
        src = Path(scratch) / name
        if src.exists():
            shutil.copyfile(src, Path(out_dir) / name)
            copied.append(name)
        else:
            print("[forge] warning: launcher %s missing in scratch" % name)
    return copied


def write_notes(spec, stats, out_dir, proof, stripped):
    rows = ["| region | generator | archetype/pattern | theme | role | "
            "origin | seed | nodes | objs |",
            "|---|---|---|---|---|---|---|---|---|"]
    for reg in spec["regions"]:
        s = stats["regions"][reg["id"]]
        rows.append("| %s | %s | %s/%s | %s | %s | [%s] | %d | %d | %d |" % (
            reg["id"], reg["generator"], reg["archetype"] or "-",
            derive_pattern(reg) if reg["generator"] == "archetype" else "-",
            reg["theme"], reg["role"],
            ",".join("%g" % v for v in reg["origin"]), s["seed"],
            s["nodes"], s["objs"]))
    proof_note = ("verdict=%s fill=%s%% colors=%s interactives=%s "
                  "missing=%s" % (proof["verdict"], proof["fill"],
                                  proof["colors"], proof["interactives"],
                                  proof["missing"])) if proof else "skipped"
    links_txt = ", ".join("%s->%s" % (a, b) for a, b in plan_links(spec))
    (out_dir / "NOTES.md").write_text(NL.join([
        "# NOTES - %s (WorldForge fusion)" % spec["name"], "",
        "- built by: world_forge.py (CDR-011), schema %s" % SCHEMA,
        "- spec seed: %d | spawn region: %s" % (spec["seed"],
                                                spec["spawn_region"]),
        "- objective: %s" % objective_text(spec),
        "- links: %s -> %d portal gate nodes" % (links_txt or "-",
                                                 stats["portals"]),
        "- duplicate player/start nodes stripped from non-spawn regions: %d"
        % stripped,
        "- gates: lint clean | littcli validate ok:true | native proof: %s"
        % proof_note,
        "- play: ENGINE.bat/.sh (Vulkan player) | VIEW.bat (C++ viewer)", "",
        "## Regions", ""] + rows + [""]), encoding="utf-8")
    (out_dir / "ATTRIBUTION.md").write_text(
        "# ATTRIBUTION - %s%s%sAll assets procedurally generated by Litt "
        "worldgen tools (per-region provenance in assets/asset_index.json). "
        "No third-party content.%s" % (spec["name"], NL, NL, NL),
        encoding="utf-8")


# ---------------------------------------------------------------------- cli
def register_manifest(spec, out_dir):
    """Record the FUSED game in Project/games.json (region scratches are
    built with LITT_NO_MANIFEST=1 and never touch the manifest)."""
    manifest_p = HERE.parent.parent.parent / "Project" / "games.json"
    try:
        manifest = json.loads(manifest_p.read_text(encoding="utf-8"))
    except Exception:
        return                      # out-of-repo deploys skip provenance
    try:
        rel_dir = str(out_dir.relative_to(manifest_p.parent.parent))
    except ValueError:
        rel_dir = str(out_dir)
    manifest["games"] = [g for g in manifest["games"]
                         if g.get("name") != spec["name"]]
    manifest["games"].append({
        "name": spec["name"], "dir": rel_dir,
        "worldforge": True,
        "regions": {r["id"]: {"generator": r["generator"],
                              "theme": r["theme"], "role": r["role"],
                              "seed": region_seed(spec, r["id"]),
                              "origin": r["origin"]}
                    for r in spec["regions"]},
        "spawn_region": spec["spawn_region"],
        "about": spec.get("about", ""),
        "built": datetime.datetime.now().isoformat(timespec="seconds")})
    manifest_p.write_text(json.dumps(manifest, indent=2), encoding="utf-8")


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="WorldForge composer: fuse a litt.worldforge/1 spec "
                    "into one explorable game (CDR-011)")
    ap.add_argument("spec", help="path to world_spec.json")
    ap.add_argument("--out-dir", default=None,
                    help="default Project/<spec.name>")
    ap.add_argument("--force", action="store_true",
                    help="replace an existing output game dir")
    ap.add_argument("--skip-native-proof", action="store_true",
                    help="skip the rendered-pixel proof gate (still lints "
                         "+ validates natively)")
    args = ap.parse_args(argv)

    def fail(msg, **extra):
        print(json.dumps({"ok": False, "error": msg, **extra}))
        return 1

    try:
        spec = load_spec(args.spec)
    except (SpecError, ValueError, OSError) as exc:
        return fail("spec invalid: %s" % exc)
    print("[forge] spec ok: %s | %d regions | seed %d"
          % (spec["name"], len(spec["regions"]), spec["seed"]))

    out_dir = Path(args.out_dir) if args.out_dir \
        else REPO / "Project" / spec["name"]
    if out_dir.exists():
        if not args.force:
            return fail("output %s exists (use --force)" % out_dir,
                        game=str(out_dir))
        if out_dir.is_dir():
            shutil.rmtree(out_dir)
        else:
            out_dir.unlink()

    sroot = scratch_root(spec)
    scratches = {r["id"]: sroot / r["id"] for r in spec["regions"]}
    try:
        sroot.mkdir(parents=True, exist_ok=True)
    except OSError as exc:
        return fail("scratch unavailable: %s" % exc)

    try:
        for reg in spec["regions"]:
            build_region(spec, reg, scratches[reg["id"]],
                         region_seed(spec, reg["id"]))
    except ForgeError as exc:
        keep = sroot if sroot.exists() else None
        print("[forge] region build FAILED; scratch kept at %s" % keep)
        return fail(str(exc)[:2000], scratch=str(keep) if keep else None)

    stats = fuse_spec_into_game(spec, scratches, out_dir)
    try:
        _report, _sim, proof = run_gates(out_dir, args.skip_native_proof)
    except ForgeError as exc:
        print(json.dumps({"ok": False, "game": str(out_dir),
                          "stage": "gates", "error": str(exc)[:2000]}))
        return 1
    deploy_launchers(scratches[spec["regions"][0]["id"]], out_dir)
    write_notes(spec, stats, out_dir, proof,
                stats["stripped_spawn_duplicates"])
    register_manifest(spec, out_dir)

    shutil.rmtree(sroot, ignore_errors=True)   # success: scratch disposable

    final = {"ok": True, "game": str(out_dir),
             "regions": {rid: {"nodes": s["nodes"], "objs": s["objs"]}
                         for rid, s in stats["regions"].items()},
             "portals": stats["portals"],
             "native_proof": proof if proof else {"skipped": True}}
    print(json.dumps(final))
    return 0


if __name__ == "__main__":
    sys.exit(main())
