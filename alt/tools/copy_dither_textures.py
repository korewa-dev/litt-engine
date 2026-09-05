// Dither3D Texture Copy Script
// Copies PNG files from Dither3D Unity package to Litt Engine assets directory
// Run this after cloning Dither3D or extracting the assets

import os
import shutil

# Source paths (from Dither3D Unity package)
SOURCE_DIR = os.environ.get('DITHER3D_SOURCE', r'D:\Allgemein\Downloads\Dither3D-main\Dither3D-main\Assets\Dither3D')

# Destination paths
DEST_DIR = r'D:\Allgemein\Documents\Default Project\litt engine\assets\dither3d'

# Files to copy
TEXTURES = [
    'Dither3D_1x1.png',
    'Dither3D_1x1_Ramp.png',
    'Dither3D_2x2.png',
    'Dither3D_2x2_Ramp.png',
    'Dither3D_4x4.png',
    'Dither3D_4x4_Ramp.png',
    'Dither3D_8x8.png',
    'Dither3D_8x8_Ramp.png',
]

def main():
    # Create destination directory
    os.makedirs(DEST_DIR, exist_ok=True)
    print(f"Destination: {DEST_DIR}")

    copied = 0
    for tex in TEXTURES:
        src = os.path.join(SOURCE_DIR, tex)
        dst = os.path.join(DEST_DIR, tex)

        if os.path.exists(src):
            shutil.copy2(src, dst)
            size = os.path.getsize(dst)
            print(f"  Copied: {tex} ({size:,} bytes)")
            copied += 1
        else:
            print(f"  MISSING: {src}")

    print(f"\nCopied {copied}/{len(TEXTURES)} textures")
    print(f"Shader files: shaders/dither3d/")
    print(f"Documentation: docs/rendering/dither3d.md")

if __name__ == '__main__':
    main()
