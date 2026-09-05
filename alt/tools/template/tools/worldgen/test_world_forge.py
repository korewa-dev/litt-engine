#!/usr/bin/env python3
"""Tests for world_forge.py (CDR-011 composer).

Plain asserts, stdlib only, no subprocesses: fuses hand-built fixture
regions (tiny OBJs + scenes + state) so every merge rule is checked
directly. Run: python template/tools/worldgen/test_world_forge.py

Covered (per CDR-011 gate list):
  - spec validation: link resolution errors (+ roles/generators/themes,
    spacing, reachability, unknown keys)
  - duplicate spawn stripping count
  - node offset math (x/z offset by region origin, y untouched)
  - name collision merge: two regions both having coin.obj -> each region
    keeps its own prefixed copy (<a>__coin.obj / <b>__coin.obj)
  - portal pair math/placement, link dedupe, goal_gate reuse vs shared arch
  - objective composition from objective_chain_hint
  - fused fixture output is lint_game-clean and scene-valid
"""
import json
import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import world_forge as wf  # noqa: E402


# ------------------------------------------------------------------ helpers
def base_spec():
    return {
        "schema": "litt.worldforge/1",
        "name": "test-realm",
        "about": "a test realm of two halves",
        "seed": 7,
        "regions": [
            {"id": "a", "generator": "archetype",
             "archetype": "precision_action", "pattern": "corridor_run",
             "theme": "cyberpunk_neon", "role": "start",
             "origin": [0, 0, 0], "links": ["b"], "size": 54},
            {"id": "b", "generator": "space", "theme": "space_station_core",
             "role": "finale", "origin": [120, 0, 0], "links": [],
             "size": 96},
        ],
        "spawn_region": "a",
        "objective_chain_hint": [
            "1. start: run corridor 'a', then head on",
            "2. finale: finish at station 'b'",
        ],
    }


def tiny_obj(name, n=3):
    """Minimal lint-clean ORIGIN-CENTERED OBJ (matches worldkit's prop
    convention, so fused scenes never smell of double-transforms).
    Extra vertices ride up y only -> distinct content per region."""
    lines = ["mtllib materials.mtl", "o %s" % name,
             "v -1 0 -1", "v 1 0 -1", "v 0 1 1"]
    for k in range(3, n):
        lines.append("v 0 %d 0" % k)
    lines += ["g %s" % name, "usemtl m1", "f 1 2 3"]
    return "\n".join(lines) + "\n"


MTL = "\n".join(["newmtl m1", "Ka 1 1 1", "Kd 0.5 0.5 0.5",
                 "Ks 0.05 0.05 0.05", "Ns 8.0"]) + "\n"


def scene_json():
    return {
        "format": "litt-scene", "version": 1, "root_id": 0, "next_id": 5,
        "nodes": [
            {"name": "Root", "id": 0, "parent": None,
             "children": [1, 2, 3, 4],
             "position": [0, 0, 0], "rotation": [0, 0, 0, 1],
             "scale": [1, 1, 1], "visible": True, "layer": 0, "tags": []},
            {"name": "Level", "id": 1, "parent": 0, "children": [],
             "position": [5, 0, 0], "rotation": [0, 0, 0, 1],
             "scale": [1, 1, 1], "visible": True, "layer": 0,
             "tags": ["floor", "model:layout_main"]},
            {"name": "Coin_01", "id": 2, "parent": 0, "children": [],
             "position": [1, 0, 2], "rotation": [0, 0, 0, 1],
             "scale": [1, 1, 1], "visible": True, "layer": 0,
             "tags": ["pickup", "score", "model:coin"]},
            {"name": "Player_Start", "id": 3, "parent": 0, "children": [],
             "position": [2, 0, 3], "rotation": [0, 0, 0, 1],
             "scale": [1, 1, 1], "visible": True, "layer": 0,
             "tags": ["player", "start"]},
            {"name": "Goal_Banner", "id": 4, "parent": 0, "children": [],
             "position": [9, 0.2, 0], "rotation": [0, 0, 0, 1],
             "scale": [1, 1, 1], "visible": True, "layer": 0,
             "tags": ["goal", "poi", "model:banner"]},
        ],
    }


STATE = {
    "format": "litt-live-state", "version": 1, "mode": "ai-exclusive",
    "theme": "cyberpunk_neon",
    "identity": {"movement": "parkour_movement"},
    "gameplay": {"physics": {"gravity": 30}, "spawn": [2.0, 0.0, 3.0]},
}

INDEX = {"format": "litt-asset-index", "version": 1, "assets": [
    {"id": stem, "type": "model", "path": "models/%s.obj" % stem,
     "loader": "load"} for stem in ("layout_main", "coin", "banner")]}


def build_region(root, coin_vertices=3):
    root = Path(root)
    models = root / "assets" / "models"
    scenes = root / "assets" / "scenes"
    models.mkdir(parents=True, exist_ok=True)
    scenes.mkdir(parents=True, exist_ok=True)
    (models / "layout_main.obj").write_text(
        tiny_obj("layout_main", n=coin_vertices), encoding="utf-8")
    (models / "coin.obj").write_text(
        tiny_obj("coin", n=coin_vertices), encoding="utf-8")
    (models / "banner.obj").write_text(tiny_obj("banner"), encoding="utf-8")
    (models / "materials.mtl").write_text(MTL, encoding="utf-8")
    (scenes / "world.lscn.json").write_text(
        json.dumps(scene_json(), indent=2) + "\n", encoding="utf-8")
    (root / "world_state.json").write_text(
        json.dumps(STATE, indent=2) + "\n", encoding="utf-8")
    (root / "assets" / "asset_index.json").write_text(
        json.dumps(INDEX, indent=2) + "\n", encoding="utf-8")


def make_scratches(tmp):
    """Fixture scratch dirs: a(start,[0,0,0]) + b(finale,[120,0,0]), both
    carrying layout_main/coin/banner with DISTINCT coin contents."""
    scratches = {}
    for rid in ("a", "b"):
        rdir = os.path.join(tmp, "region_%s" % rid)
        build_region(rdir, coin_vertices=3 if rid == "a" else 6)
        scratches[rid] = rdir
    return scratches


def fuse_fixture(tmp):
    spec = base_spec()
    scratches = make_scratches(tmp)
    out = os.path.join(tmp, "fused")
    stats = wf.fuse_spec_into_game(spec, scratches, out)
    return spec, scratches, out, stats


def read_json(path):
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def expect_spec_error(mutate, *needle):
    spec = base_spec()
    mutate(spec)
    try:
        wf.validate_spec(spec, themes={"cyberpunk_neon",
                                       "space_station_core"},
                         archetypes={"precision_action"})
    except wf.SpecError as exc:
        msg = str(exc)
        assert all(n in msg for n in needle), \
            "%r not fully reported in:\n%s" % (needle, msg)
        return
    raise AssertionError("SpecError not raised (needles %r)" % (needle,))


# ------------------------------------------------------------------- tests
def test_offset_math():
    assert wf.offset_pos([35, 6, 0], [120, 0, 0]) == [155.0, 6.0, 0.0]
    assert wf.offset_pos([35, 6, 4], [-10, 99, -2]) == [25.0, 6.0, 2.0]
    # y is NEVER offset (region-local ground), x/z always are
    assert wf.offset_pos([0, 0, 0], [5, 5, 5]) == [5.0, 0.0, 5.0]
    assert wf.offset_pos([1.5, 2.5, 3.5], [0.25, 0, 0.25]) == \
        [1.75, 2.5, 3.75]


def test_validate_link_resolution_errors():
    expect_spec_error(lambda s: s["regions"][0].__setitem__(
        "links", ["ghost"]), "V016", "'ghost'")
    expect_spec_error(lambda s: s["regions"][0].__setitem__(
        "links", ["b", "b"]), "V016", "duplicate link targets")
    expect_spec_error(lambda s: s["regions"][0].__setitem__(
        "links", ["a"]), "V016", "self-link")
    expect_spec_error(lambda s: s.__setitem__("spawn_region", "zzz"),
                      "V022")


def test_validate_schema_roles_generators_themes_keys():
    expect_spec_error(lambda s: s.__setitem__("schema", "litt.worldforge/2"),
                      "V002")
    expect_spec_error(lambda s: s["regions"][0].__setitem__("role", "boss"),
                      "V013")

    def two_starts(s):
        s["regions"][1]["role"] = "start"
    expect_spec_error(two_starts, "V020")       # second start

    def two_finales(s):
        s["regions"][0]["role"] = "finale"
    expect_spec_error(two_finales, "V021")      # second finale
    expect_spec_error(lambda s: s["regions"][1].__setitem__(
        "generator", "voxel"), "V011")
    expect_spec_error(lambda s: s["regions"][0].__setitem__(
        "theme", "not_a_theme"), "V012")
    expect_spec_error(lambda s: s["regions"][1].__setitem__(
        "archetype", "walking_simulator"),
        "V017", "only allowed when generator=archetype")
    expect_spec_error(lambda s: s["regions"][0].__setitem__(
        "archetype", "not_an_archetype"), "V017")
    expect_spec_error(lambda s: s["regions"][0].__setitem__(
        "pattern", "zigzag"), "V018")
    expect_spec_error(lambda s: s.__setitem__("oops", 1),
                      "V026", "unknown top-level key 'oops'")
    expect_spec_error(lambda s: s["regions"][0].__setitem__("oops", 1),
                      "V026", "unknown region key 'oops'")
    expect_spec_error(lambda s: s.__setitem__(
        "objective_chain_hint", ["only one"]), "V023")
    expect_spec_error(lambda s: s.__setitem__("seed", True),
                      "V005")

    def dup_ids(s):
        s["regions"] = [dict(s["regions"][0])] * 3
        s["regions"][1] = dict(s["regions"][0])
        s["regions"][2] = dict(s["regions"][0])
    expect_spec_error(dup_ids, "V010", "duplicate id")


def test_validate_spacing_and_reachability():
    def too_close(s):
        s["regions"][1]["origin"] = [10, 0, 0]   # needs >= (54+96)/2 = 75
    expect_spec_error(too_close, "V024")

    def unreachable(s):                          # drop the a->b link
        s["regions"][0]["links"] = []
    expect_spec_error(unreachable, "V025",
                      "unreachable from spawn_region: b")


def test_valid_spec_roundtrip_and_defaults():
    spec = wf.validate_spec(base_spec(),
                            themes={"cyberpunk_neon",
                                    "space_station_core"},
                            archetypes={"precision_action"})
    assert spec["spawn_region"] == "a"
    assert wf.region_seed(spec, "a") == 7
    assert wf.region_seed(spec, "b") == 8
    assert spec["regions"][0]["origin"] == [0.0, 0.0, 0.0]
    # archetype region WITHOUT pattern derives one from design structure
    reg = dict(base_spec()["regions"][0])
    reg.pop("pattern")
    derived = wf.derive_pattern(reg)
    assert derived in wf.PATTERNS


def test_duplicate_spawn_stripping_count():
    with tempfile.TemporaryDirectory(prefix="wf-test-") as tmp:
        # three-region chain a(start)->b(middle)->c(middle): every
        # non-spawn region's player/start must be stripped -> count 2
        spec3 = base_spec()
        spec3["regions"].append(
            {"id": "c", "generator": "tabletop",
             "theme": "minimalist_abstract", "role": "middle",
             "origin": [240, 0, 0], "links": [], "size": 48})
        spec3["regions"][1]["links"] = ["c"]
        scratches = make_scratches(tmp)
        cdir = os.path.join(tmp, "region_c")
        build_region(cdir, coin_vertices=9)
        scratches["c"] = cdir
        # input census: THREE regions, EACH shipping one player/start node,
        # so the only correct strip count for this fixture is 2 (b's + c's)
        assert len(spec3["regions"]) == 3
        for rid in ("a", "b", "c"):
            rscene = read_json(os.path.join(scratches[rid], "assets",
                                            "scenes", "world.lscn.json"))
            spawns = [n for n in rscene["nodes"]
                      if set(n.get("tags", [])) >= {"player", "start"}]
            assert len(spawns) == 1, "%s fixture must ship one spawn" % rid
        out = os.path.join(tmp, "fused3chain")
        stats = wf.fuse_spec_into_game(spec3, scratches, out)
        scene = read_json(os.path.join(out, "assets", "scenes",
                                       "world.lscn.json"))
        players = [n for n in scene["nodes"]
                   if set(n.get("tags", [])) >= {"player", "start"}]
        names = {n["name"] for n in scene["nodes"]}
        assert len(players) == 1, "expected exactly ONE player/start node"
        assert players[0]["name"] == "a__Player_Start"
        assert players[0]["position"] == [2.0, 0.0, 3.0]
        # both non-spawn spawns really were dropped BY NAME
        assert "b__Player_Start" not in names
        assert "c__Player_Start" not in names
        assert stats["stripped_spawn_duplicates"] == 2, stats


def test_node_offset_in_fused_scene():
    with tempfile.TemporaryDirectory(prefix="wf-test-") as tmp:
        _spec, _scratches, out, _stats = fuse_fixture(tmp)
        scene = read_json(os.path.join(out, "assets", "scenes",
                                       "world.lscn.json"))
        names = {n["name"]: n for n in scene["nodes"]}
        # region b nodes shifted by origin x+120, z+0; y untouched
        assert names["b__Level"]["position"] == [125.0, 0.0, 0.0]
        assert names["b__Coin_01"]["position"] == [121.0, 0.0, 2.0]
        assert names["b__Goal_Banner"]["position"] == [129.0, 0.2, 0.0]
        # region a sits at its own origin
        assert names["a__Level"]["position"] == [5.0, 0.0, 0.0]
        # ids re-rooted flat under Root, next_id consistent
        assert scene["nodes"][0]["children"] == list(
            range(1, len(scene["nodes"])))
        assert scene["next_id"] == len(scene["nodes"])
        for n in scene["nodes"][1:]:
            assert n["parent"] == 0 and n["children"] == []


def test_name_collision_merge():
    with tempfile.TemporaryDirectory(prefix="wf-test-") as tmp:
        _spec, _scratches, out, stats = fuse_fixture(tmp)
        models = os.path.join(out, "assets", "models")
        listing = sorted(os.listdir(models))
        # both regions shipped coin.obj -> BOTH survive prefixed, no bare
        assert listing == [
            "a__banner.obj", "a__coin.obj", "a__layout_main.obj",
            "a__materials.mtl", "b__banner.obj", "b__coin.obj",
            "b__layout_main.obj", "b__materials.mtl",
            "wf_goal_gate.obj", "wf_portal.mtl"], listing
        a_coin = open(os.path.join(models, "a__coin.obj")).read()
        b_coin = open(os.path.join(models, "b__coin.obj")).read()
        assert "v 0 4 0" in b_coin and "v 0 4 0" not in a_coin, \
            "distinct contents must stay distinct"
        # mtllib rewritten to each region's namespaced MTL copy
        assert a_coin.startswith("mtllib a__materials.mtl")
        assert b_coin.startswith("mtllib b__materials.mtl")
        scene = read_json(os.path.join(out, "assets", "scenes",
                                       "world.lscn.json"))
        refs = {t for n in scene["nodes"] for t in n.get("tags", [])
                if t.startswith("model:")}
        assert {"model:a__coin", "model:b__coin"} <= refs
        index = read_json(os.path.join(out, "assets", "asset_index.json"))
        prov = {e["id"]: e.get("provenance", {}).get("region")
                for e in index["assets"]}
        assert prov.get("a__coin") == "a" and prov.get("b__coin") == "b"
        assert stats["regions"]["a"]["objs"] == 3
        assert stats["regions"]["b"]["objs"] == 3


def test_portal_pair_math():
    pa, pb, ya, yb = wf.portal_placement([0, 0, 0], [120, 0, 0])
    assert pa == [56.0, 0.0, 0.0] and pb == [64.0, 0.0, 0.0]  # mid +/- 4m
    assert ya == 90.0 and yb == -90.0          # facing along the axis
    pa2, pb2, _, _ = wf.portal_placement([0, 0, 0], [0, 0, 80])
    assert pa2 == [0.0, 0.0, 36.0] and pb2 == [0.0, 0.0, 44.0]
    # degenerate identical origins fall back to +x deterministically
    qa, qb, _, _ = wf.portal_placement([5, 1, 5], [5, 1, 5])
    assert qa[0] < qb[0]


def test_portal_nodes_and_dedupe():
    with tempfile.TemporaryDirectory(prefix="wf-test-") as tmp:
        spec, _scratches, out, stats = fuse_fixture(tmp)
        scene = read_json(os.path.join(out, "assets", "scenes",
                                       "world.lscn.json"))
        portals = [n for n in scene["nodes"]
                   if "portal" in n.get("tags", [])]
        assert len(portals) == 2                  # one pair -> TWO gates
        assert all("goal" in n["tags"] for n in portals)
        xs = sorted(n["position"][0] for n in portals)
        assert xs == [56.0, 64.0]
        # shared arch emitted once (fixtures ship no kit goal_gate)
        assert os.path.exists(os.path.join(out, "assets", "models",
                                           "wf_goal_gate.obj"))
        assert stats["portals"] == 2
        # link dedupe: adding b->a must NOT add more gates
        spec2 = base_spec()
        spec2["regions"][1]["links"] = ["a"]
        scratches = make_scratches(tmp)
        out2 = os.path.join(tmp, "fused2")
        stats2 = wf.fuse_spec_into_game(spec2, scratches, out2)
        scene2 = read_json(os.path.join(out2, "assets", "scenes",
                                        "world.lscn.json"))
        portals2 = [n for n in scene2["nodes"]
                    if "portal" in n.get("tags", [])]
        assert len(portals2) == 2 and stats2["portals"] == 2


def test_goal_gate_mesh_reuse():
    """A kit goal_gate mesh in ANY region wins: portals reference its
    namespaced copy and NO shared arch is emitted."""
    with tempfile.TemporaryDirectory(prefix="wf-test-") as tmp:
        spec, scratches, _out, _stats = fuse_fixture(tmp)
        models_a = Path(scratches["a"]) / "assets" / "models"
        (models_a / "goal_gate.obj").write_text(tiny_obj("goal_gate"),
                                                encoding="utf-8")
        (Path(scratches["a"]) / "assets" / "asset_index.json").write_text(
            json.dumps({"format": "litt-asset-index", "version": 1,
                        "assets": [{"id": "goal_gate", "type": "model",
                                    "path": "models/goal_gate.obj",
                                    "loader": "load"}]}), encoding="utf-8")
        ref, need_arch = wf.portal_ref({"a": ["goal_gate"], "b": []})
        assert ref == "a__goal_gate" and need_arch is False
        out3 = os.path.join(tmp, "fused3")
        st3 = wf.fuse_spec_into_game(spec, scratches, out3)
        assert not os.path.exists(os.path.join(out3, "assets", "models",
                                               "wf_goal_gate.obj"))
        scene3 = read_json(os.path.join(out3, "assets", "scenes",
                                        "world.lscn.json"))
        ptags = {t for n in scene3["nodes"]
                 if "portal" in n.get("tags", []) for t in n["tags"]}
        assert "model:a__goal_gate" in ptags
        index3 = read_json(os.path.join(out3, "assets", "asset_index.json"))
        assert any(e["id"] == "a__goal_gate" for e in index3["assets"])
        assert st3["regions"]["a"]["objs"] == 4


def test_objective_composition():
    spec = base_spec()
    text = wf.objective_text(spec)
    assert "1. start: run corridor 'a'" in text
    assert "2. finale: finish at station 'b'" in text
    spec2 = base_spec()
    spec2["objective_chain_hint"] = None
    assert wf.objective_text(spec2) == "journey across 2 regions: a -> b"


def test_derive_pattern_from_structure():
    reg = {"generator": "archetype", "archetype": "precision_action",
           "pattern": None}
    assert wf.derive_pattern(reg) in wf.PATTERNS   # never crashes, in vocab
    reg2 = {"generator": "archetype", "archetype": "precision_action",
            "pattern": "corridor_run"}
    assert wf.derive_pattern(reg2) == "corridor_run"  # explicit wins


def test_fused_fixture_lint_clean():
    from lint import lint_game, solid_count
    with tempfile.TemporaryDirectory(prefix="wf-test-") as tmp:
        _spec, _scratches, out, _stats = fuse_fixture(tmp)
        report = lint_game(out)
        assert not report["problems"], report["problems"]
        assert not report["dangling_refs"], report["dangling_refs"]
        scene_path = os.path.join(out, "assets", "scenes",
                                  "world.lscn.json")
        assert solid_count(scene_path) == 2       # one walkable per region
        state = read_json(os.path.join(out, "world_state.json"))
        assert state["gameplay"]["objective"].startswith("1. start:")
        assert state["gameplay"]["spawn"] == [2.0, 0.0, 3.0]
        assert state["meta"]["worldforge"]["portals"] == 2
        assert state["seed"]["worldforge_seed"] == 7


def test_plan_links_order_and_uniqueness():
    spec = base_spec()
    spec["regions"][1]["links"] = ["a"]
    assert wf.plan_links(spec) == [("a", "b")]  # deduped, first-seen order


def main():
    tests = [(k, v) for k, v in sorted(globals().items())
             if k.startswith("test_")]
    for name, fn in tests:
        fn()
        print("PASS %s" % name)
    print("ALL %d TESTS PASSED" % len(tests))


if __name__ == "__main__":
    main()
