#!/usr/bin/env python3
"""gen_texture.py - generate a texture asset for a Litt game.

Primary path: Stable Diffusion via an A1111-compatible server
(--server or the `ai_assets.endpoint` in litt_engine.json).

Fallback path (--fallback or when the server is unreachable): a
deterministic procedural pattern derived from the prompt hash, so AI
pipelines never stall just because no GPU server is up. The output is
clearly logged as fallback.

Registers into the project's asset_index.json (kind=texture) and can
patch materials.mtl to bind map_Kd onto one or more materials.

Usage:
  python gen_texture.py --game-dir Project/kingsfall-hollow \
      --name crypt_stone --prompt "seamless dark gothic stone wall, moss" \
      [--mtl crypt_stone,paving] [--seed 7] [--size 512]
"""
import argparse
import base64
import datetime
import hashlib
import json
import math
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).parent.parent / "worldgen"))
from worldkit import register_index  # noqa: E402
from sd_client import SDClient, SDError  # noqa: E402

TEXTURE_BUDGET_KB = 256


def read_config_endpoint():
    """Pick up ai_assets.endpoint from litt_engine.json when present."""
    for cand in ("litt_engine.json", os.path.expanduser("~/.litt/litt_engine.json")):
        try:
            data = json.loads(Path(cand).read_text(encoding="utf-8"))
            ep = (data.get("ai_assets") or {}).get("endpoint")
            if ep:
                return ep
        except Exception:
            pass
    return None


# ------------------------------------------------------------------ fallback
def procedural_png(prompt, size, seed):
    """Deterministic seamless-ish value-noise texture; returns PNG bytes.

    Uses pygame (already a repo dependency) for encoding."""
    import pygame  # local import: only needed on the fallback path
    rng = hashlib.sha256((prompt + "|" + str(seed)).encode("utf-8")).digest()

    def noise(ix, iy, octaves=4):
        v, amp, freq = 0.0, 1.0, 1.0 / 16.0
        h = hashlib.sha256(rng + bytes([ix & 255, iy & 255])).digest()
        base = h[0] / 255.0
        for o in range(octaves):
            gx, gy = int(ix * freq), int(iy * freq)
            hh = hashlib.sha256(
                rng + bytes([o, (gx * 73 + gy * 151) & 255])).digest()
            v += (hh[0] / 255.0) * amp
            amp *= 0.5
            freq *= 2.0
        return v, base

    surf = pygame.Surface((size, size))
    palette = [(rng[i] / 255.0) for i in range(6, 9)]
    lo, hi = 0.35, 0.85
    px = pygame.surfarray.pixels3d(surf)
    step = max(1, size // 256)
    for y in range(size):
        for x in range(0, size, step):
            n, _ = noise(x, y)
            t = lo + (hi - lo) * n
            r = min(255, int(255 * t * palette[0]))
            g = min(255, int(255 * t * palette[1]))
            b = min(255, int(255 * t * palette[2]))
            for k in range(step):
                if x + k < size:
                    px[x + k, y] = (r, g, b)
    del px

    import io
    buf = io.BytesIO()
    pygame.image.save(surf, buf, "t.png")
    return buf.getvalue()


# ------------------------------------------------------------------ budget
def enforce_budget(png_bytes, max_kb=TEXTURE_BUDGET_KB):
    """Downscale until under budget; returns possibly-smaller PNG bytes."""
    import io
    import pygame
    if len(png_bytes) <= max_kb * 1024:
        return png_bytes
    img = pygame.image.load(io.BytesIO(png_bytes))
    w, h = img.get_size()
    while len(png_bytes) > max_kb * 1024 and w > 64 and h > 64:
        w, h = w // 2, h // 2
        scaled = pygame.transform.smoothscale(img, (w, h))
        buf = io.BytesIO()
        pygame.image.save(scaled, buf, "t.png")
        png_bytes = buf.getvalue()
        img = scaled
    return png_bytes


# ------------------------------------------------------------------ mtl patch
def patch_mtl(mtl_path, material_names, rel_tex_path):
    """Bind map_Kd on each named material (append newmtl when absent)."""
    text = mtl_path.read_text(encoding="utf-8") if mtl_path.exists() else ""
    blocks = {}
    order = []
    cur = None
    for line in text.splitlines():
        if line.strip().startswith("newmtl "):
            cur = line.strip().split(None, 1)[1]
            blocks[cur] = [line]
            order.append(cur)
        elif cur is not None:
            blocks[cur].append(line)
    for name in material_names:
        name = name.strip()
        if not name:
            continue
        lines = blocks.get(name)
        tex_line = "\tmap_Kd %s" % rel_tex_path
        if lines is None:
            blocks[name] = ["newmtl " + name,
                            "\tKd 0.5 0.5 0.5", tex_line]
            order.append(name)
        elif not any("map_Kd" in ln for ln in lines):
            out = []
            inserted = False
            for ln in lines:
                out.append(ln)
                if not inserted and ln.strip().startswith(("Kd ", "Ns ", "Ka ")):
                    out.append(tex_line)
                    inserted = True
            if not inserted:
                out.append(tex_line)
            blocks[name] = out
    body = "\n".join("\n".join(blocks[n]) for n in order) + "\n"
    mtl_path.write_text(body, encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--name", required=True, help="texture id, e.g. crypt_stone")
    ap.add_argument("--prompt", required=True)
    ap.add_argument("--negative", default="blurry, watermark, text, jpeg artifacts")
    ap.add_argument("--size", type=int, default=512)
    ap.add_argument("--steps", type=int, default=20)
    ap.add_argument("--cfg", type=float, default=7.0)
    ap.add_argument("--seed", type=int, default=-1)
    ap.add_argument("--server", default=None,
                    help="A1111-compatible base URL (default: config file)")
    ap.add_argument("--api-key", default=None)
    ap.add_argument("--model", default=None, help="checkpoint title to select")
    ap.add_argument("--mtl", default="",
                    help="comma list of material names to bind map_Kd onto")
    ap.add_argument("--fallback", action="store_true",
                    help="skip the server entirely, use procedural")
    a = ap.parse_args()

    root = Path(a.game_dir)
    tex_dir = root / "assets" / "textures"
    tex_dir.mkdir(parents=True, exist_ok=True)

    source = "stable-diffusion"
    png = None
    if a.fallback:
        source = "procedural-fallback"
    else:
        server = a.server or read_config_endpoint() or "http://127.0.0.1:7860"
        client = SDClient(server, a.api_key)
        try:
            if not client.health():
                raise SDError("health check failed")
            png = client.txt2img(a.prompt, a.negative, a.size, a.size,
                                 a.steps, a.cfg, a.seed, model=a.model)
        except SDError as e:
            print("[texture] SD unavailable (%s)" % e)
            print("[texture] falling back to procedural pattern")
            source = "procedural-fallback"
    if png is None:
        seed = a.seed if a.seed >= 0 else 1337
        png = procedural_png(a.prompt, min(a.size, 512), seed)

    png = enforce_budget(png)
    rel_path = "textures/%s.png" % a.name
    out = root / "assets" / rel_path
    out.write_bytes(png)

    register_index(root / "assets", a.name, "assets/" + rel_path,
                   loader="litt_asset::manager::AssetManager::load_texture",
                   kind="texture")

    if a.mtl:
        patch_mtl(root / "assets" / "models" / "materials.mtl",
                  a.mtl.split(","), "../" + rel_path)

    # provenance log
    with open(root / "LIVE_LOG.md", "a", encoding="utf-8") as fh:
        fh.write("\n---\n\n## %s - TEXTURE by ai-agent (AI)\n"
                 % datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))
        fh.write("- asset: %s (%s, %d KB)\n" % (rel_path, source,
                                                len(png) // 1024))
        fh.write("- prompt: %s\n" % a.prompt[:120])

    print("[texture] %s <- %s (%d KB)%s" % (
        rel_path, source, len(png) // 1024,
        " -> bound to %s" % a.mtl if a.mtl else ""))


if __name__ == "__main__":
    main()
