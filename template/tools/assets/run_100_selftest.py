#!/usr/bin/env python3
"""Run asset selftest 100 iterations."""
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).parent.parent.parent
TOTAL = 100
passed = failed = 0
times = []

for i in range(1, TOTAL + 1):
    t0 = time.perf_counter()
    r = subprocess.run(
        [sys.executable, str(ROOT / "template/tools/assets/selftest.py")],
        capture_output=True, text=True, timeout=60
    )
    dt = time.perf_counter() - t0
    times.append(dt)
    if r.returncode == 0:
        passed += 1
    else:
        failed += 1
        print(f"  Iter {i} FAILED")
        print(r.stdout[-500:] if len(r.stdout) > 500 else r.stdout)
    if i % 20 == 0 or i == 100:
        print(f"  Iter {i:3d}/100: {passed} passed, {failed} failed, {dt*1000:.0f}ms")

print()
print(f"SELFTEST: {passed}/100 passed, {failed}/100 failed")
print(f"Avg time: {sum(times)/len(times)*1000:.0f}ms")
sys.exit(0 if failed == 0 else 1)
