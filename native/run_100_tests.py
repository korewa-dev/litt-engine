#!/usr/bin/env python3
"""Run Litt Engine tests 100 iterations and report results."""
import subprocess
import sys
import time
from pathlib import Path

TESTS_EXE = Path(__file__).parent / "tests.exe"
TOTAL_ITERATIONS = 100

def run_tests():
    """Run tests.exe and return (passed, failed, output)."""
    try:
        result = subprocess.run(
            [str(TESTS_EXE)],
            capture_output=True,
            text=True,
            timeout=30,
            cwd=Path(__file__).parent
        )
        output = result.stdout + result.stderr
        # Parse passed/failed from output
        passed = 0
        failed = 0
        for line in output.splitlines():
            if "Passed:" in line:
                parts = line.split()
                for i, p in enumerate(parts):
                    if p == "Passed:" and i + 1 < len(parts):
                        passed = int(parts[i + 1])
                    if p == "Failed:" and i + 1 < len(parts):
                        failed = int(parts[i + 1])
        return passed, failed, output, result.returncode
    except subprocess.TimeoutExpired:
        return 0, 0, "TIMEOUT", 1
    except Exception as e:
        return 0, 0, str(e), 1

def main():
    if not TESTS_EXE.exists():
        print(f"ERROR: {TESTS_EXE} not found")
        sys.exit(1)

    print(f"Running Litt Engine tests for {TOTAL_ITERATIONS} iterations...")
    print("=" * 60)

    total_passed = 0
    total_failed = 0
    iteration_failures = []
    iteration_times = []

    for i in range(1, TOTAL_ITERATIONS + 1):
        start = time.perf_counter()
        passed, failed, output, rc = run_tests()
        elapsed = time.perf_counter() - start
        iteration_times.append(elapsed)

        total_passed += passed
        total_failed += failed

        if failed > 0:
            iteration_failures.append((i, failed, output, rc))
            status = f"FAIL ({failed} failures)"
        else:
            status = "PASS"

        # Progress bar (ASCII for compatibility)
        bar_len = 30
        filled = int(bar_len * i / TOTAL_ITERATIONS)
        bar = "=" * filled + "-" * (bar_len - filled)
        print(f"[{bar}] {i:3d}/{TOTAL_ITERATIONS} | {status:20s} | {elapsed*1000:.0f}ms", flush=True)

    print("=" * 60)
    avg_time = sum(iteration_times) / len(iteration_times)
    min_time = min(iteration_times)
    max_time = max(iteration_times)

    print(f"\nResults: {TOTAL_ITERATIONS} iterations")
    print(f"  Total passed assertions: {total_passed}")
    print(f"  Total failed assertions: {total_failed}")
    print(f"  Iterations with failures: {len(iteration_failures)}")
    print(f"  Avg time/iter: {avg_time*1000:.0f}ms")
    print(f"  Min time/iter: {min_time*1000:.0f}ms")
    print(f"  Max time/iter: {max_time*1000:.0f}ms")

    if iteration_failures:
        print(f"\n{'=' * 60}")
        print("FAILURE DETAILS:")
        for iter_num, failed, output, rc in iteration_failures:
            print(f"\n--- Iteration {iter_num} ({failed} failures, rc={rc}) ---")
            print(output[:500])

        print(f"\n{'=' * 60}")
        print("TESTS FAILED")
        sys.exit(1)
    else:
        print(f"\n{'=' * 60}")
        print("ALL TESTS PASSED")
        sys.exit(0)

if __name__ == "__main__":
    main()
