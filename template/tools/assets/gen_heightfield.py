#!/usr/bin/env python3
"""gen_heightfield.py - turn ANY image into a Litt terrain mesh.

Feed it a Stable Diffusion output (gen_texture.py), a photo, or anything:
luminance becomes elevation. Emits a grid OBJ registered in the project's
asset_index, ready for a `model:<name>` node tag.

Usage:
  python gen_heightfield.py --game-dir Project/ember-depths \
      --image assets/textures/ash_terrain.png --name ash_terrain \
      [--size 24] [--height 4] [--res 64]
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / "worldgen"))
from worldkit import register_index  # noqa: E402


def luminance_grid(png_path, res):
    """Sample the image into an (res+1)x(res+1) height field in [0,1]."""
    import pygame
    img = pygame.image.load(str(png_path))
    w, h = img.get_size()
    grid = []
    for j in range(res + 1):
        row = []
        sy = min(h - 1, int(j * h / res))
        for i in range(res + 1):
            sx = min(w - 1, int(i * w / res))
            r, g, b, _a = img.get_at((sx, sy))
            row.append((0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0)
        grid.append(row)
    return grid


def write_heightmap_obj(path, name, grid, size_m, height_m):
    """Emit a displaced grid centered at origin; returns face count."""
    res_y = len(grid) - 1
    res_x = len(grid[0]) - 1
    lines = ["# %s: AI heightfield terrain" % name,
             "mtllib materials.mtl", "o " + name]

    # vertices (y-up): x right, z forward
    half = size_m / 2.0
    for j, row in enumerate(grid):
        z = -half + size_m * j / res_y
        for i, hval in enumerate(row[:res_x + 1]):
            x = -half + size_m * i / res_x
            y = hval * height_m
            lines.append("v %.4f %.4f %.4f" % (x, y, z))

    def vid(i, j):
        return j * (res_x + 1) + i + 1

    lines.append("usemtl prop_void")
    faces = 0
    for j in range(res_y):
        for i in range(res_x):
            a, b = vid(i, j), vid(i + 1, j)
            c, d = vid(i + 1, j + 1), vid(i, j + 1)
            lines.append("f %d// %d// %d//" % (a, c, b))
            lines.append("f %d// %d// %d//" % (a, d, c))
            faces += 2
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return faces


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--image", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--size", type=float, default=24.0, help="meters across")
    ap.add_argument("--height", type=float, default=4.0, help="max relief m")
    ap.add_argument("--res", type=int, default=64)
    a = ap.parse_args()

    root = Path(a.game_dir)
    image = root / "assets" / a.image.lstrip("/")
    if not image.exists():
        raise SystemExit("image not found: %s" % image)

    grid = luminance_grid(image, a.res)
    models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    obj_path = models / (a.name + ".obj")
    faces = write_heightmap_obj(obj_path, a.name, grid, a.size, a.height)
    register_index(root / "assets", a.name, "models/%s.obj" % a.name)

    print("[terrain] %s <- %s | %dx%d grid, %d faces, %.1fm relief"
          % (obj_path.name, a.image, a.res, a.res, faces, a.height))


if __name__ == "__main__":
    main()
