#!/usr/bin/env python3
"""Run all Litt Engine tests 100 iterations and report results."""
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).parent.parent
TESTS_EXE = ROOT / "native" / "tests.exe"
SELFTEST = ROOT / "template" / "tools" / "assets" / "selftest.py"
WORLDGEN_TESTS = [
    ROOT / "template" / "tools" / "worldgen" / "test_gen_props.py",
    ROOT / "template" / "tools" / "worldgen" / "test_worldkit.py",
    ROOT / "template" / "tools" / "worldgen" / "test_world_forge.py",
    ROOT / "template" / "tools" / "worldgen" / "test_world_planner.py",
]
TOTAL = 100

def run_test(cmd, cwd=None, label=""):
    try:
        start = time.perf_counter()
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60, cwd=cwd)
        elapsed = time.perf_counter() - start
        passed = "PASS" if result.returncode == 0 else "FAIL"
        return passed, elapsed, result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        return "TIMEOUT", 0, "Timeout"
    except Exception as e:
        return f"ERROR: {e}", 0, str(e)

def main():
    print(f"Running Litt Engine full test suite for {TOTAL} iterations...")
    print("=" * 70)

    suites = [
        ([str(TESTS_EXE)], "native/tests.exe", None),
        ([sys.executable, str(SELFTEST)], "asset/selftest.py", None),
    ]
    for t in WORLDGEN_TESTS:
        suites.append(([sys.executable, str(t)], f"worldgen/{t.name}", t.parent))

    total_suites = len(suites)
    all_results = {name: {"pass": 0, "fail": 0} for _, name, _ in suites}
    all_times = {name: [] for _, name, _ in suites}
    failures_log = {name: [] for _, name, _ in suites}

    for i in range(1, TOTAL + 1):
        for (cmd, name, cwd), suit_idx in zip(suites, range(total_suites)):
            result, elapsed, output = run_test(cmd, cwd, name)
            all_times[name].append(elapsed)
            if result == "PASS":
                all_results[name]["pass"] += 1
            else:
                all_results[name]["fail"] += 1
                failures_log[name].append((i, result, output[:200]))

        # Progress
        bar_len = 30
        filled = int(bar_len * i / TOTAL)
        bar = "=" * filled + "-" * (bar_len - filled)
        sys.stdout.write(f"\r[{bar}] {i:3d}/{TOTAL}  ")
        sys.stdout.flush()

    print()  # newline after progress
    print("=" * 70)
    print(f"RESULTS: {TOTAL} iterations x {total_suites} test suites")
    print("=" * 70)

    all_pass = True
    for (cmd, name, cwd), _ in zip(suites, range(total_suites)):
        r = all_results[name]
        t = all_times[name]
        avg = sum(t) / len(t) if t else 0
        status = "OK" if r["fail"] == 0 else f"FAIL ({r['fail']} failures)"
        if r["fail"] > 0:
            all_pass = False
        print(f"  {name:30s} | {r['pass']:3d}/{r['pass']+r['fail']} pass | avg {avg*1000:.0f}ms | {status}")

        if failures_log[name]:
            print(f"    First failure at iteration {failures_log[name][0][0]}: {failures_log[name][0][1][:100]}")

    print("=" * 70)
    if all_pass:
        print("ALL TESTS PASSED")
        sys.exit(0)
    else:
        print("SOME TESTS FAILED")
        sys.exit(1)

if __name__ == "__main__":
    main()
