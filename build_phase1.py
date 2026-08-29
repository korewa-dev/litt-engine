#!/usr/bin/env python3
"""Build Phase 1 tests for Litt Engine"""
import subprocess
import sys
import os

# Paths
ROOT = "D:/Allgemein/AI Router/litt engine"
GPP = r"C:\Users\roika\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260616-ucrt-x86_64\bin\g++.exe"
BIN = os.path.join(ROOT, "native", "bin")

# Ensure bin directory exists
os.makedirs(BIN, exist_ok=True)

# Compiler flags
FLAGS = [
    "-std=c++17", "-O0", "-g", "-Wall", "-Wextra",
    f"-I{ROOT}", f"-I{os.path.join(ROOT, 'include')}",
    "-DLITT_NULL_DEVICE=1"
]

# Files to compile
files = [
    "native/littcore/litt_memory.h",
    "native/littcore/litt_event.h",
    "native/littcore/litt_gpu.h",
    "native/littcore/litt_scene_graph.h",
    "native/littcore/litt_memory.cpp",
    "native/littcore/litt_event.cpp",
    "native/littcore/litt_gpu.cpp",
    "native/littcore/litt_scene_graph.cpp",
    "native/littcore/litt_phase1_tests.cpp",
    "native/littcore/litt_math.h",
    "native/littcore/litt_json.h",
    "native/littcore/litt_obj.h",
]

# Compile each file
print("Compiling Phase 1...")
for f in files:
    if f.endswith('.h'):
        continue
    out = os.path.join(BIN, f.split('/')[-1].replace('.cpp', '.o'))
    cmd = [GPP, *FLAGS, "-c", f, "-o", out]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"ERROR compiling {f}:")
        print(result.stderr)
        sys.exit(1)
    print(f"  ✓ {f.split('/')[-1]}")

# Link
print("\nLinking...")
objs = [
    os.path.join(BIN, "litt_memory.o"),
    os.path.join(BIN, "litt_event.o"),
    os.path.join(BIN, "litt_gpu.o"),
    os.path.join(BIN, "litt_scene_graph.o"),
    os.path.join(BIN, "litt_phase1_tests.o"),
    os.path.join(BIN, "litt_math.o"),
    os.path.join(BIN, "litt_json.o"),
    os.path.join(BIN, "litt_obj.o"),
]
exe = os.path.join(BIN, "litt_phase1_tests.exe")
cmd = [GPP, *FLAGS, *objs, "-o", exe]
result = subprocess.run(cmd, capture_output=True, text=True)
if result.returncode != 0:
    print("ERROR linking:")
    print(result.stderr)
    sys.exit(1)
print(f"  ✓ Linked {exe}")

# Run tests
print("\nRunning tests...")
result = subprocess.run([exe], capture_output=True, text=True)
print(result.stdout)
if result.stderr:
    print("STDERR:", result.stderr)
sys.exit(result.returncode)
