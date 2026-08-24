#!/usr/bin/env python3
"""Tests for worldkit Placement registry + transform-convention guards.
Plain asserts, stdlib only. Run: python template/tools/worldgen/test_worldkit.py"""
import json
import math
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import worldkit as wk


def approx(a, b, tol=1e-9):
    return abs(a - b) <= tol


def test_rng_determinism():
    r1, r2 = wk.Rng(4242), wk.Rng(4242)
    seq1 = [r1.next_u32() for _ in range(64)]
    seq2 = [r2.next_u32() for _ in range(64)]
    assert seq1 == seq2
    r3 = wk.Rng(4243)
    seq3 = [r3.next_u32() for _ in range(64)]
    assert seq1 != seq3


def test_center_box():
    mn, mx = wk.center_box(10.0, -4.0, 4.0, 2.0)
    assert mn == (8.0, -5.0) and mx == (12.0, -3.0)


def test_placement_insert_rejects_overlap():
    reg = wk.Placement()
    assert reg.insert("a", (0, 0), (2, 2)) is True
    # full overlap -> rejected, state untouched
    assert reg.insert("b", (1, 1), (3, 3)) is False
    assert len(reg) == 1
    # partial overlap on one axis only -> rejected
    assert reg.insert("c", (5, 0), (7, 2)) is True   # disjoint first
    assert reg.insert("d", (-2, 1), (1, 4)) is False  # touches a's x edge
    # edge-touching counts as overlap (documented inclusive policy)...
    assert reg.conflicts((2, 0), (4, 2)) == ["a"]
    # ...shrink slightly -> accepted
    assert reg.insert("e", (2.001, 0), (4, 2)) is True
    # duplicate name rejected even when box differs
    assert reg.insert("a", (100, 100), (101, 101)) is False
    # reversed corners are normalized
    assert reg.insert("f", (20, 20), (18, 22)) is True
    assert reg.bounds("f")[0] == (18.0, 20.0) and reg.bounds("f")[1] == (20.0, 22.0)


def test_placement_conflicts_order_and_ignore():
    reg = wk.Placement()
    reg.insert("first", (0, 0), (10, 10))
    reg.insert("second", (20, 0), (30, 10))
    reg.insert("third", (40, 0), (50, 10))
    q = ((5, 0), (45, 5))
    assert reg.conflicts(*q) == ["first", "third"]  # insertion order
    assert reg.conflicts(*q, ignore=("first",)) == ["third"]
    assert reg.conflicts((60, 60), (61, 61)) == []
    assert list(reg) == ["first", "second", "third"]
    assert reg.names() == ("first", "second", "third")
    assert "second" in reg and "nope" not in reg


def test_ground_snap():
    reg = wk.Placement()
    assert reg.ground_y(0, 0) == 0.0                      # empty -> default
    assert reg.insert("floor", (-10, -10), (10, 10), top=0.4, walkable=True)
    assert reg.insert("crate", (-1, -1), (1, 1), top=1.4, walkable=True)
    assert reg.insert("ghost", (-2, -2), (2, 2), top=9.9, walkable=False)
    assert approx(reg.ground_y(0, 0), 1.4)                # max walkable top
    assert approx(reg.ground_y(5, 5), 0.4)                # floor only
    assert reg.contains(0, 0) and reg.contains(5, 5)
    assert not reg.contains(50, 50)
    assert reg.ground_y(50, 50, default=-3.0) == -3.0     # explicit default
    # ground never blocks; solids do
    assert reg.insert("rock", (3, 3), (4, 4)) is True
    assert reg.insert("rock2", (3.5, 3), (5, 4)) is False
    assert reg.conflicts((3.5, 3), (5, 4)) == ["rock"]
    assert reg.conflicts((0, 0), (2, 2)) == ["ghost"]     # solid still blocks
    assert reg.conflicts((-10, -10), (-9, -9)) == []      # ground never blocks
    # walkable + blocks = standable platform: provides top AND rejects
    assert reg.insert("deck2", (10, 10), (12, 12), top=2.5,
                      walkable=True, blocks=True)
    assert approx(reg.ground_y(11, 11), 2.5)
    assert reg.insert("prop_inside_deck", (10.5, 10.5), (10.7, 10.7)) is False


def test_reserve_spot():
    reg = wk.Placement()
    reg.insert("deck", (-6, -2), (6, 2), top=0.5, walkable=True)
    pos = wk.reserve_spot(reg, "Coin_01", 3.0, 0.0, 0.8, 0.8, lift=1.2)
    assert pos == [3.0, 1.7, 0.0]                         # 0.5 deck top + lift
    assert "Coin_01" in reg                               # footprint committed
    blocked = wk.reserve_spot(reg, "Coin_02", 3.0, 0.0, 0.8, 0.8)
    assert blocked is None                                # overlapping Coin_01
    off_deck = wk.reserve_spot(reg, "Rock_01", 50.0, 50.0, 1.0, 1.0,
                               y_default=2.0)
    assert off_deck == [50.0, 2.0, 50.0]                  # y_default fallback


def make_box(mb_like_center):
    mb = wk.MeshBuilder()
    mb.begin("p", "m")
    cx, cy_off = mb_like_center
    mb.box(cx, 0.25 + cy_off, cx, 0.25, 0.25, 0.25)  # unit-ish crate
    return mb


def test_centroid_and_assertion():
    mb = make_box((0.0, 0.0))          # built at origin, base y=0
    c = wk.mesh_centroid(mb)
    assert approx(c[0], 0.0) and approx(c[2], 0.0)
    assert approx(c[1], 0.25)          # y rides up the mesh - that is correct
    assert wk.assert_origin_centered(mb) is True                 # xz default
    assert wk.assert_origin_centered(mb, axes="xz") is True
    try:
        wk.assert_origin_centered(mb, axes="xyz")
        raise AssertionError("xyz must flag y centroid 0.25")
    except wk.TransformError:
        pass
    bad = make_box((3.0, 0.0))         # world coords baked in -> the bug
    try:
        wk.assert_origin_centered(bad)
        raise AssertionError("offset mesh must fail")
    except wk.TransformError as e:
        assert "x=" in str(e) or "z=" in str(e)
    assert approx(wk.mesh_centroid([[1, 2, 3], [3, 2, 1]])[0], 2.0)  # raw rows
    try:
        wk.mesh_centroid(wk.MeshBuilder())
        raise AssertionError("empty mesh must raise")
    except ValueError:
        pass
    try:
        wk.assert_origin_centered(mb, axes="q")
        raise AssertionError("unknown axes must raise")
    except ValueError:
        pass


def test_translate_and_recenter():
    mb = make_box((4.0, 0.0))
    off = wk.recenter_mesh(mb)                       # xz only by default
    assert approx(off[0], -4.0) and off[1] == 0.0 and approx(off[2], -4.0)
    assert wk.assert_origin_centered(mb) is True
    assert approx(wk.mesh_centroid(mb)[1], 0.25)     # base height preserved
    mb2 = make_box((0.0, 0.0))
    mb2.translate(1, 2, 3)
    c = wk.mesh_centroid(mb2)
    assert (approx(c[0], 1.0), approx(c[1], 2.25), approx(c[2], 3.0)) == (True,) * 3
    off2 = wk.recenter_mesh(mb2, axes="xyz")
    assert approx(off2[1], -2.25)                    # y included this time
    assert approx(wk.mesh_centroid(mb2)[1], 0.0)


def test_save_prop_guards(tmp):
    good = make_box((0.0, 0.0))
    p, kb, nf = wk.save_prop(tmp, "ok_crate", good, "materials",
                             {"m": (0.5, 0.5, 0.5)}, enforce_origin=True)
    assert p.name == "ok_crate.obj" and nf > 0 and kb > 0
    bad = make_box((9.0, 0.0))
    target = os.path.join(str(tmp), "bad_crate.obj")
    try:
        wk.save_prop(tmp, "bad_crate", bad, "materials", {})
        raise AssertionError("enforce_origin must reject baked coords")
    except wk.TransformError:
        assert not os.path.exists(target)             # nothing was written
    fixme = make_box((9.0, 0.0))
    p2, _, _ = wk.save_prop(tmp, "fixed_crate", fixme, "materials", {},
                            auto_recenter=True)
    verts = [[float(t[1]), float(t[2]), float(t[3])]
             for t in (l.split() for l in open(p2, encoding="utf-8"))
             if t[0] == "v"]
    cx = sum(v[0] for v in verts) / len(verts)
    cz = sum(v[2] for v in verts) / len(verts)
    assert abs(cx) < 0.01 and abs(cz) < 0.01          # repaired to origin
    try:
        wk.save_prop(tmp, "x", make_box((0, 0)), "materials", {},
                     enforce_origin=True, auto_recenter=True)
        raise AssertionError("mutually exclusive flags must raise")
    except ValueError:
        pass


def test_write_scene_compat_and_tags(tmp):
    p = os.path.join(str(tmp), "compat.lscn.json")
    wk.write_scene(p, [
        ("Coin_07", [1, 2, 3], 90, ["pickup"]),
        ("Crate", [0, 0, 0], 0, ["solid"], "crate"),
    ], "compat")
    data = json.load(open(p, encoding="utf-8"))
    assert data["format"] == "litt-scene" and data["next_id"] == 3
    coin = data["nodes"][1]
    assert coin["tags"] == ["pickup", "model:coin_07"]      # tag injected
    crate = data["nodes"][2]
    assert "model:crate" in crate["tags"]                    # explicit ref kept
    assert approx(crate["rotation"][1], math.sin(math.radians(0) / 2))


def test_write_scene_placement(tmp):
    reg = wk.Placement()
    reg.insert("deck", (-6, -2), (6, 2), top=0.5, walkable=True)
    p = os.path.join(str(tmp), "placed.lscn.json")
    wk.write_scene(p, [
        ("Coin_01", [3.0, 1.7, 0.0], 0, ["pickup"], "coin", (0.4, 0.4)),
        ("Coin_02", [-3.0, 1.7, 0.0], 0, ["pickup"], "coin", (0.4, 0.4)),
    ], "placed", placement=reg)
    assert set(reg.names()) >= {"deck", "Coin_01", "Coin_02"}
    # conflict against registry -> ValueError AND no file rewrite
    before = open(p, encoding="utf-8").read()
    n_before = len(reg)
    try:
        wk.write_scene(p, [
            ("Wall", [3.0, 0.0, 0.0], 0, ["solid"], "wall", (2.0, 2.0)),
        ], "clash", placement=reg)
        raise AssertionError("registry conflict must raise")
    except ValueError as e:
        assert "Coin_01" in str(e)
    assert open(p, encoding="utf-8").read() == before       # disk untouched
    assert len(reg) == n_before                             # registry untouched
    # batch-internal overlap caught too (even far from any registry entry)
    try:
        wk.write_scene(os.path.join(str(tmp), "clash2.lscn.json"), [
            ("A", [100, 0, 100], 0, [], None, (1, 1)),
            ("B", [100.5, 0, 100], 0, [], None, (1, 1)),
        ], "clash2", placement=reg)
        raise AssertionError("batch overlap must raise")
    except ValueError as e:
        assert "A" in str(e) and "B" in str(e)
    # duplicate name inside one batch (same name, different footprint)
    try:
        wk.write_scene(os.path.join(str(tmp), "clash3.lscn.json"), [
            ("Dup", [200, 0, 200], 0, [], None, (1, 1)),
            ("Dup", [220, 0, 220], 0, [], None, (1, 1)),
        ], "clash3", placement=reg)
        raise AssertionError("duplicate node name must raise")
    except ValueError as e:
        assert "already registered" in str(e)
    # idempotent rewrite: reserve_spot-tracked node re-passes cleanly
    pos = wk.reserve_spot(reg, "Idem", -50.0, -50.0, 1.0, 1.0)
    wk.write_scene(os.path.join(str(tmp), "idem.lscn.json"),
                   [("Idem", pos, 0, ["pickup"], "idem", (0.5, 0.5))],
                   "idem", placement=reg)
    # footprints are optional - unfootprinted nodes bypass checks (legacy use)
    wk.write_scene(os.path.join(str(tmp), "nofp.lscn.json"),
                   [("X", [3.0, 0, 0.0], 0, ["poi"])], "nofp", placement=reg)


def test_byte_determinism(tmp):
    def build(seed):
        rng = wk.Rng(seed)
        mb = wk.MeshBuilder()
        mb.begin("prop", "mat")
        mb.box(0, 0.25, 0, 0.25, 0.25, 0.25)
        mb.cone(rng.uniform(-0.3, 0.3), 0.5, rng.uniform(-0.3, 0.3),
                rng.uniform(0.1, 0.35), 0.5,
                seg=rng.pick([6, 8, 10, 12, 16]))
        return mb.to_obj("prop", "materials")[0]

    def scene_bytes(path, seed):
        rng = wk.Rng(seed)
        reg = wk.Placement()
        placed = []
        for i in range(5):
            pos = wk.reserve_spot(reg, "N%02d" % i,
                                  round(rng.uniform(-4, 4), 2),
                                  round(rng.uniform(-2, 2), 2), 1.0, 1.0)
            if pos:
                placed.append(("N%02d" % i, pos, 0, ["pickup"], "n%02d" % i,
                               (0.5, 0.5)))
        wk.write_scene(path, placed, "det", placement=reg)
        return open(path, "rb").read()

    # same seed -> byte-identical mesh text
    assert build(7) == build(7)
    # separated seeds -> divergent output (coarse quantization can collide,
    # so probe several and require at least one difference)
    assert any(build(s) != build(7) for s in (101, 202, 303, 404))
    pa = os.path.join(str(tmp), "det_a.json")
    pb = os.path.join(str(tmp), "det_b.json")
    assert scene_bytes(pa, 7) == scene_bytes(pb, 7)


def main():
    tests = [(k, v) for k, v in sorted(globals().items())
             if k.startswith("test_")]
    with tempfile.TemporaryDirectory(prefix="wk-test-") as tmp:
        for name, fn in tests:
            takes_tmp = "tmp" in fn.__code__.co_varnames[:fn.__code__.co_argcount]
            if takes_tmp:
                fn(tempfile.mkdtemp(dir=tmp))
            else:
                fn()
            print("PASS %s" % name)
    print("ALL %d TESTS PASSED" % len(tests))


if __name__ == "__main__":
    main()
