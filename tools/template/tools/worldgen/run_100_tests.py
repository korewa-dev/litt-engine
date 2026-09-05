#!/usr/bin/env python3
"""Run worldgen tests 100 iterations."""
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).parent.parent.parent.parent
WORLDGEN_DIR = ROOT / "template/tools/worldgen"
TOTAL = 100
tests = ["test_gen_props.py", "test_worldkit.py", "test_world_forge.py", "test_world_planner.py"]
all_passed = True

for test in tests:
    passed = failed = 0
    times = []
    for i in range(1, TOTAL + 1):
        t0 = time.perf_counter()
        r = subprocess.run(
            [sys.executable, str(WORLDGEN_DIR / test)],
            capture_output=True, text=True, timeout=30
        )
        dt = time.perf_counter() - t0
        times.append(dt)
        if r.returncode == 0:
            passed += 1
        else:
            failed += 1
            all_passed = False
    avg = sum(times) / len(times) * 1000
    print(f"{test:25s}: {passed}/100 passed, avg {avg:.0f}ms")

print()
status = "ALL PASSED" if all_passed else "FAILURES DETECTED"
print(f"WORLDGEN: {status}")
sys.exit(0 if all_passed else 1)
