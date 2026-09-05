#!/usr/bin/env python3
"""Tests for world_planner.py - WorldForge planning half (CDR-011).

Covers the gates from docs/specs/WORLDFORGE_SPEC.md:
  - schema round-trip (plan -> write -> validate -> reload)
  - determinism (same --about/--seed -> byte-identical spec, cross-process)
  - keyword mapping assertions for 6 phrases
  - validator catches broken JSON / bad role / unknown generator /
    missing link target / spacing / spawn / unknown keys
Plain asserts, stdlib only.
Run: python template/tools/worldgen/test_world_planner.py
"""
import copy
import json
import math
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import world_planner as wp                                    # noqa: E402

PLANNER = os.path.abspath(wp.__file__)


def run_cli(args):
    return subprocess.run([sys.executable, PLANNER] + list(args),
                          capture_output=True, text=True, encoding="utf-8",
                          errors="replace")


def last_json_line(stdout):
    lines = [ln for ln in stdout.strip().splitlines() if ln.strip()]
    return json.loads(lines[-1])


def themes_of(spec):
    return [r["theme"] for r in spec["regions"]]


def archs_of(spec):
    return [r["archetype"] for r in spec["regions"]]


def patterns_of(spec):
    return [r["pattern"] for r in spec["regions"]]


# ------------------------------------------------------------------- tests
def test_schema_roundtrip(tmp):
    spec = wp.plan_world("a frozen kingdom with a volcanic arena", seed=7,
                         name="frost-realm")
    path = os.path.join(str(tmp), "roundtrip.json")
    payload = json.dumps(spec, indent=2, sort_keys=True) + "\n"
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(payload)
    violations = wp.validate_spec_file(path)
    assert violations == [], violations
    with open(path, "r", encoding="utf-8") as fh:
        reloaded = json.load(fh)
    assert reloaded == spec
    assert reloaded["schema"] == wp.SCHEMA_ID
    assert reloaded["spawn_region"] == spec["regions"][0]["id"]
    # planner output file validates too (CLI-level round trip)
    out = os.path.join(str(tmp), "cli.json")
    proc = run_cli(["--about", "sky island farming", "--seed", "11",
                    "--out", out])
    assert proc.returncode == 0, proc.stderr
    assert wp.validate_spec_file(out) == []


def test_determinism(tmp):
    a1 = wp.plan_world("haunted mall survival", seed=42)
    a2 = wp.plan_world("haunted mall survival", seed=42)
    assert a1 == a2
    b1 = os.path.join(str(tmp), "det_a.json")
    b2 = os.path.join(str(tmp), "det_b.json")
    p1 = run_cli(["--about", "desert racing festival", "--seed", "9",
                  "--out", b1])
    p2 = run_cli(["--about", "desert racing festival", "--seed", "9",
                  "--out", b2])
    assert p1.returncode == 0 and p2.returncode == 0
    raw1 = open(b1, "rb").read()
    raw2 = open(b2, "rb").read()
    assert raw1 == raw2, "same --about/--seed must be byte-identical"
    # default seed is fixed -> even flag-less runs reproduce
    c1 = os.path.join(str(tmp), "det_c.json")
    c2 = os.path.join(str(tmp), "det_c2.json")
    run_cli(["--about", "underwater ruins heist", "--out", c1])
    run_cli(["--about", "underwater ruins heist", "--out", c2])
    assert open(c1, "rb").read() == open(c2, "rb").read()


def test_keyword_mapping_phrases():
    vocab = wp.load_vocab()
    phrases = [
        "a frozen kingdom with a volcanic arena",
        "space station horror",
        "haunted mall survival",
        "desert racing festival",
        "underwater ruins heist",
        "sky island farming",
    ]
    specs = {}
    for phrase in phrases:
        spec = wp.plan_world(phrase, seed=13)
        specs[phrase] = spec
        roles = [r["role"] for r in spec["regions"]]
        assert roles[0] == "start" and roles[-1] == "finale", phrase
        assert all(t in vocab["theme_set"] for t in themes_of(spec)), phrase
        assert all(a in vocab["arch_set"]
                   for a in archs_of(spec) if a), phrase
        assert all(p in wp.PATTERNS
                   for p in patterns_of(spec) if p), phrase

    s = specs["a frozen kingdom with a volcanic arena"]
    assert len(s["regions"]) >= 2
    assert s["regions"][0]["theme"] == "arctic_expanse"      # frozen
    assert s["regions"][0]["archetype"] == "open_world_rpg"  # kingdom
    assert s["regions"][-1]["theme"] == "dark_fantasy"       # volcanic/fire
    assert s["regions"][-1]["pattern"] == "arena_ring"       # arena
    assert s["regions"][-1]["archetype"] == "character_action"

    s = specs["space station horror"]
    assert {r["generator"] for r in s["regions"]} == {"space"}
    assert s["regions"][0]["theme"] == "space_station_core"
    assert "haunted_estate" in themes_of(s)

    s = specs["haunted mall survival"]
    assert s["regions"][0]["theme"] == "haunted_estate"
    assert "open_world_survival" in archs_of(s)
    assert "modern_city_day" in themes_of(s)          # the mall

    s = specs["desert racing festival"]
    assert "desert_dunes" in themes_of(s)
    assert "kart_racer" in archs_of(s)
    assert "spline_track" in patterns_of(s)

    s = specs["underwater ruins heist"]
    assert s["regions"][0]["theme"] == "underwater_reef"
    assert "coop_heist" in archs_of(s)

    s = specs["sky island farming"]
    assert s["regions"][0]["theme"] == "sky_islands"
    assert "farming_sim" in archs_of(s)
    assert "grid_board" in patterns_of(s)


def test_region_count_and_loop():
    for k in (2, 3, 4, 5):
        spec = wp.plan_world("a frozen kingdom with a volcanic arena",
                             seed=21, k=k)
        assert len(spec["regions"]) == k
        roles = [r["role"] for r in spec["regions"]]
        assert roles.count("start") == 1 and roles.count("finale") == 1
        assert roles.count("middle") == k - 2
        chain = ([spec["regions"][0]["id"]] +
                 [lid for r in spec["regions"][:-1] for lid in r["links"]])
        assert chain[-1] == spec["regions"][-1]["id"], "chain must end finale"
    plain = wp.plan_world("space station horror", seed=1, k=3, loop=False)
    assert plain["regions"][-1]["links"] == []
    looped = wp.plan_world("space station horror", seed=1, k=3, loop=True)
    assert looped["regions"][-1]["links"] == [plain["regions"][0]["id"]]
    assert wp.validate_spec(looped) == []
    assert len(looped["objective_chain_hint"]) == 3


def test_layout_spacing_property():
    for about, seed in [("a frozen kingdom with a volcanic arena", 7),
                        ("space station horror", 3),
                        ("underwater ruins heist", 5),
                        ("sky island farming", 77)]:
        for k in (2, 3, 4, 5):
            spec = wp.plan_world(about, seed=seed, k=k)
            regs = spec["regions"]
            for i in range(len(regs)):
                for j in range(i + 1, len(regs)):
                    oi, oj = regs[i]["origin"], regs[j]["origin"]
                    d = math.sqrt(sum((oi[c] - oj[c]) ** 2 for c in range(3)))
                    need = (regs[i]["size"] + regs[j]["size"]) / 2.0
                    assert d >= need, (
                        "%s k=%d pair %d,%d: %.1f < %.1f"
                        % (about, k, i, j, d, need))
            assert all(o[1] == 0 for o in (r["origin"] for r in regs))
            if k != 4:
                xs = [r["origin"][0] for r in regs]
                assert xs == sorted(xs) and len(set(xs)) == k


def make_base_spec():
    return wp.plan_world("a frozen kingdom with a volcanic arena", seed=7)


def expect_codes(mutator, codes):
    spec = mutator(make_base_spec())
    found = {v["code"] for v in wp.validate_spec(spec)}
    missing = set(codes) - found
    assert not missing, "expected %s, got %s" % (sorted(codes), sorted(found))


def test_validator_catches_bad_role():
    def bad_finale(spec):
        spec["regions"][-1]["role"] = "boss"
        return spec
    expect_codes(bad_finale, ["V013", "V021"])

    def no_start(spec):
        spec["regions"][0]["role"] = "middle"
        return spec
    expect_codes(no_start, ["V020", "V022"])


def test_validator_catches_unknown_generator():
    def mutate(spec):
        spec["regions"][0]["generator"] = "minecraft"
        return spec
    expect_codes(mutate, ["V011", "V017", "V018"])


def test_validator_catches_missing_link_target():
    def mutate(spec):
        spec["regions"][0]["links"] = ["ghost-region"]
        return spec
    expect_codes(mutate, ["V016", "V025"])


def test_validator_catches_broken_and_drifted_specs(tmp):
    broken = os.path.join(str(tmp), "broken.json")
    with open(broken, "w", encoding="utf-8") as fh:
        fh.write('{"schema": "litt.worldforge/1", "regions": [oops}')
    try:
        wp.validate_spec_file(broken)
        assert False, "broken JSON must raise SpecJsonError"
    except wp.SpecJsonError:
        pass
    proc = run_cli(["--spec-in", broken])
    assert proc.returncode == 1
    assert last_json_line(proc.stdout)["ok"] is False

    spec = make_base_spec()

    def dup(spec):                      # duplicate region id
        spec["regions"][1]["id"] = spec["regions"][0]["id"]
        spec["regions"][0]["links"] = [spec["regions"][1]["id"]]
        spec["regions"][1]["links"] = [spec["regions"][2]["id"]]
        return spec
    expect_codes(dup, ["V010"])

    def theme(spec):                    # unknown theme
        spec["regions"][0]["theme"] = "lava_land"
        return spec
    expect_codes(theme, ["V012"])

    def spawn(spec):                    # spawn not on start region
        spec["spawn_region"] = spec["regions"][-1]["id"]
        return spec
    expect_codes(spawn, ["V022"])

    def spacing(spec):                  # moved origin violates spacing rule
        spec["regions"][1]["origin"] = [10, 0, 0]
        return spec
    expect_codes(spacing, ["V024"])

    def unknown_key(spec):              # typo'd top-level key
        spec["regionz"] = spec.pop("regions")
        return spec
    expect_codes(unknown_key, ["V026"])

    def hint_len(spec):                 # objective hints must match regions
        spec["objective_chain_hint"] = spec["objective_chain_hint"][:-1]
        return spec
    expect_codes(hint_len, ["V023"])

    def flagship_arch(spec):            # archetype field on a flagship gen
        spec["regions"][0]["generator"] = "space"
        spec["regions"][0]["archetype"] = None
        spec["regions"][0]["pattern"] = None
        spec["regions"][1]["generator"] = "space"
        spec["regions"][1]["archetype"] = "soulslike"
        return spec
    expect_codes(flagship_arch, ["V017"])

    def schema(spec):                   # wrong schema literal
        spec["schema"] = "litt.worldforge/2"
        return spec
    expect_codes(schema, ["V002"])


def test_validate_mode_accepts_good_spec(tmp):
    path = os.path.join(str(tmp), "good.json")
    run_cli(["--about", "a frozen kingdom with a volcanic arena",
             "--seed", "7", "--name", "frost-realm", "--out", path])
    proc = run_cli(["--spec-in", path])
    assert proc.returncode == 0, proc.stdout + proc.stderr
    tail = last_json_line(proc.stdout)
    assert tail["ok"] is True and tail["violations"] == []


def test_e2e_demo_gate(tmp):
    """The CDR-011 gate command, end to end via the CLI."""
    out = os.path.join(str(tmp), "world_spec.json")
    proc = run_cli(["--about", "a frozen kingdom with a volcanic arena",
                    "--seed", "7", "--name", "frost-realm", "--out", out])
    assert proc.returncode == 0, proc.stderr
    tail = last_json_line(proc.stdout)
    assert tail["ok"] is True and tail["name"] == "frost-realm"
    assert tail["seed"] == 7 and len(tail["regions"]) >= 2
    spec = json.loads(open(out, "r", encoding="utf-8").read())
    vocab = wp.load_vocab()
    assert spec["schema"] == wp.SCHEMA_ID
    roles = [r["role"] for r in spec["regions"]]
    assert "start" in roles and "finale" in roles
    assert all(r["theme"] in vocab["theme_set"] for r in spec["regions"])
    val = run_cli(["--spec-in", out])          # validator accepts own output
    assert val.returncode == 0, val.stdout


def main():
    tests = [(k, v) for k, v in sorted(globals().items())
             if k.startswith("test_")]
    with tempfile.TemporaryDirectory(prefix="wf-test-") as tmp:
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
