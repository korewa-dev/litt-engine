# Dither3D Integration Build Script
# Compiles GLSL shaders to SPIR-V and copies assets

import os
import subprocess
import sys
from pathlib import Path

# Project root
ROOT = Path(__file__).parent.parent
SHADER_DIR = ROOT / "shaders"
ASSET_DIR = ROOT / "assets" / "dither3d"
BUILD_DIR = ROOT / "build" / "shaders"

def run(cmd, **kwargs):
    """Run command and return success"""
    result = subprocess.run(cmd, capture_output=True, text=True, **kwargs)
    if result.returncode != 0:
        print(f"  ✗ {' '.join(cmd[:3])}... failed")
        if result.stderr:
            print(f"    {result.stderr[:200]}")
        return False
    return True

def compile_glsl(src: Path, dst: Path, stage: str) -> bool:
    """Compile single GLSL shader to SPIR-V"""
    tools = ["glslangValidator", "glslc"]
    for tool in tools:
        try:
            if tool == "glslc":
                cmd = [tool, "-x", stage, str(src), "-o", str(dst)]
            else:
                cmd = [tool, "-V", "--target-env", "vulkan1.2", str(src), "-o", str(dst)]

            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode == 0:
                print(f"  ✓ {src.name} → {dst.name}")
                return True
        except FileNotFoundError:
            continue
    print(f"  ! No compiler found, skipping: {src.name}")
    return False

def main():
    print("=" * 50)
    print("  Dither3D Build Script")
    print("=" * 50)

    # 1. Create build directories
    build_dither = BUILD_DIR / "dither3d"
    build_dither.mkdir(parents=True, exist_ok=True)
    print(f"\n📁 Build directory: {build_dither}")

    # 2. Check source shaders
    dither_shaders = [
        ("mesh.vert.glsl", "dither3d_mesh_vert", "vert"),
        ("mesh.frag.glsl", "dither3d_mesh_frag", "frag"),
    ]

    compiled = 0
    for src_name, dst_name, stage in dither_shaders:
        src = SHADER_DIR / "dither3d" / src_name
        dst = build_dither / f"{dst_name}.spv"

        if src.exists():
            if compile_glsl(src, dst, stage):
                compiled += 1
        else:
            print(f"  ⚠ Missing: {src}")

    # 3. Check asset textures
    print("\n📦 Asset textures:")
    texture_files = [
        "Dither3D_1x1.png",
        "Dither3D_1x1_Ramp.png",
        "Dither3D_2x2.png",
        "Dither3D_2x2_Ramp.png",
        "Dither3D_4x4.png",
        "Dither3D_4x4_Ramp.png",
        "Dither3D_8x8.png",
        "Dither3D_8x8_Ramp.png",
    ]

    present = 0
    for tex in texture_files:
        tex_path = ASSET_DIR / tex
        if tex_path.exists():
            size = tex_path.stat().st_size
            print(f"  ✓ {tex} ({size:,} bytes)")
            present += 1
        else:
            print(f"  ⚠ Missing: {tex}")

    # 4. Summary
    print("\n" + "=" * 50)
    print(f"  Summary:")
    print(f"    Shaders compiled: {compiled}/{len(dither_shaders)}")
    print(f"    Textures found: {present}/{len(texture_files)}")
    print("=" * 50)

    if compiled == len(dither_shaders) and present == len(texture_files):
        print("\n✅ All Dither3D assets ready!")
        return 0
    elif compiled > 0:
        print("\n⚠️  Partial completion - check missing files above")
        return 1
    else:
        print("\n❌ Build failed - install glslangValidator or copy textures first")
        return 2

if __name__ == "__main__":
    sys.exit(main())
