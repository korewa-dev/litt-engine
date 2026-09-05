# Dither3D Build Script
# Compiles GLSL shaders to SPIR-V for Vulkan

import os
import subprocess
import sys
from pathlib import Path

# Directories
SHADER_DIR = Path(__file__).parent.parent / "shaders"
BUILD_DIR = Path(__file__).parent.parent / "build" / "shaders"

# Dither3D shaders
DITHER_SHADERS = [
    ("dither3d/include.glsl", "dither3d/include", None),  # Include, not compiled directly
    ("dither3d/mesh.vert.glsl", "dither3d/mesh_vert", "vert"),
    ("dither3d/mesh.frag.glsl", "dither3d/mesh_frag", "frag"),
]

def compile_glsl(src: Path, dst: Path, stage: str):
    """Compile GLSL to SPIR-V using glslangValidator"""
    # Try glslangValidator first, then glslc
    tools = ["glslangValidator", "glslc"]

    for tool in tools:
        try:
            cmd = [tool]
            if tool == "glslc":
                cmd.extend(["-x", stage, str(src), "-o", str(dst)])
            else:
                cmd.extend(["-V", "--target-env", "vulkan1.2", str(src), "-o", str(dst)])

            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode == 0:
                print(f"  ✓ {src.name} -> {dst.name}")
                return True
            else:
                print(f"  ✗ {tool} failed: {result.stderr[:200]}")
        except FileNotFoundError:
            continue

    print(f"  ! No GLSL compiler found, skipping: {src.name}")
    return False

def main():
    dither_dir = SHADER_DIR / "dither3d"
    build_dither = BUILD_DIR / "dither3d"

    if not dither_dir.exists():
        print("Error: shaders/dither3d/ not found")
        sys.exit(1)

    build_dither.mkdir(parents=True, exist_ok=True)
    print(f"Compiling Dither3D shaders to {build_dither}...")

    compiled = 0
    for src_name, dst_name, stage in DITHER_SHADERS:
        src = dither_dir / src_name
        dst = build_dither / f"{dst_name}.spv"

        if src.exists() and stage:  # Skip include files
            if compile_glsl(src, dst, stage):
                compiled += 1

    print(f"\nCompiled {compiled} shaders")
    return 0

if __name__ == "__main__":
    sys.exit(main())
