#!/usr/bin/env python3
"""Litt Native Player - a real desktop game window. No browser.

Renders any Litt-generated world (the exact files the web player uses):
  world_state.json            gameplay physics + identity + environment
  assets/scenes/world.lscn.json  nodes (position/quaternion/tags)
  assets/models/*.obj|*.mtl   triangles + flat colors

Software rasterizer: numpy transforms, painter z-sort, backface cull,
lambert shading. Perfect for Litt low-poly worlds (<50k triangles).

Controls: WASD move | Space jump | Q/E rotate camera | R respawn | Esc quit

Usage:  python play_native.py [--project DIR] [--width W] [--height H]
Headless self-test:  python play_native.py --frames 30 --dummy
"""
import argparse
import math
import os
import sys
import time
from pathlib import Path

import numpy as np
import pygame

def parse_mtl(path):
    mats = {}
    cur = None
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        t = line.strip()
        if t.startswith("newmtl "):
            cur = t[7:].strip()
            mats[cur] = (0.7, 0.7, 0.7)
        elif cur and t.startswith("Kd "):
            p = t[3:].split()
            mats[cur] = tuple(float(x) for x in p[:3])
    return mats

def parse_obj_groups(path):
    """-> [(color, verts ndarray (k,3)), ...] one entry per usemtl group."""
    vs, out, cur_mat, cur_idx = [], [], "default", []
    groups = []
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        t = line.strip()
        if not t or t.startswith("#"):
            continue
        p = t.split()
        if p[0] == "v":
            vs.append((float(p[1]), float(p[2]), float(p[3])))
        elif p[0] == "usemtl":
            if cur_idx:
                groups.append((cur_mat, cur_idx))
            cur_mat, cur_idx = p[1], []
        elif p[0] == "f":
            ids = [int(seg.split("//")[0]) - 1 for seg in p[1:]]
            for k in range(1, len(ids) - 1):
                cur_idx.append((ids[0], ids[k], ids[k + 1]))
    if cur_idx:
        groups.append((cur_mat, cur_idx))
    varr = np.asarray(vs, dtype=np.float64)
    resolved = []
    for mat, idx in groups:
        faces = np.asarray(idx, dtype=np.int64)
        resolved.append((mat, varr[faces]))
    return resolved

def quat_yaw(q):
    x, y, z, w = q
    return math.atan2(2 * (w * y + x * z), 1 - 2 * (y * y + z * z))

def rot_y(pts, ang):
    c, s = math.cos(ang), math.sin(ang)
    r = np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
    return pts @ r.T

class World:
    def __init__(self, project):
        import json
        self.state = json.loads((project / "world_state.json").read_text(encoding="utf-8"))
        self.scene = json.loads((project / "assets/scenes/world.lscn.json").read_text(encoding="utf-8"))
        self.mats = parse_mtl(project / "assets/models/materials.mtl")
        self.tris = []          # (verts (3,3) world, normal, color, node_name)
        self.solids = []        # AABB (lo (3,), hi (3,)) walkable surfaces
        self.inter = []         # dicts: name tags center slice alive base_verts
        self.node_slices = {}   # node_name -> [start_face, end_face)
        self._gather(project)
        self.V = np.zeros((0, 3))
        self._finalize()

    def _add_group(self, verts, mat, node_name, offset=(0, 0, 0), yaw=0.0):
        v = verts
        if yaw:
            v = rot_y(v, yaw)
        v = v + np.asarray(offset, dtype=np.float64)
        n = np.cross(v[:, 1] - v[:, 0], v[:, 2] - v[:, 0])
        ln = np.linalg.norm(n, axis=1, keepdims=True)
        n = n / np.maximum(ln, 1e-12)
        col = np.asarray(self.mats.get(mat, (0.7, 0.7, 0.7)))
        start = len(self.tris)
        for i in range(len(v)):
            self.tris.append((v[i], n[i], col, node_name))
        self.node_slices[node_name] = self.node_slices.get(node_name, [len(self.tris), len(self.tris)])
        self.node_slices[node_name][1] = len(self.tris)
        lo, hi = v.reshape(-1, 3).min(axis=0), v.reshape(-1, 3).max(axis=0)
        return lo, hi

    def _gather(self, project):
        solid_tags = {"floor", "level", "board", "track", "hub", "terrain", "platform"}
        inter_tags = {"pickup", "score", "goal", "hazard", "enemy", "checkpoint", "poi", "objective", "dice", "token", "player", "start", "win"}
        def handle(node_name, rel_obj, offset, yaw, tags):
            path = project / "assets" / (rel_obj + ".obj")
            if not path.exists():
                return
            lo_all = np.full(3, np.inf); hi_all = np.full(3, -np.inf)
            for mat, v in parse_obj_groups(path):
                lo, hi = self._add_group(v, mat, node_name, offset, yaw)
                lo_all = np.minimum(lo_all, lo); hi_all = np.maximum(hi_all, hi)
            center = (lo_all + hi_all) / 2.0
            if tags & solid_tags:
                self.solids.append((lo_all.copy(), hi_all.copy()))
            if tags & inter_tags:
                sl = self.node_slices[node_name]
                self.inter.append({"name": node_name, "tags": set(tags), "center": center,
                                   "slice": sl, "alive": True})
        for c in self.state.get("chunks", []):
            rel = c["path"].replace("assets/", "", 1).replace(".obj", "")
            handle(c["id"], rel, tuple(c.get("position", [0, 0, 0])), 0.0, {"terrain", "floor"})
        for node in self.scene.get("nodes", []):
            if node.get("id") == 0:
                continue
            mt = [t for t in node.get("tags", []) if t.startswith("model:")]
            if not mt:
                continue
            handle(node["name"], "models/" + mt[0][6:], tuple(node.get("position", [0, 0, 0])),
                   quat_yaw(node.get("rotation", [0, 0, 0, 1])), set(node.get("tags", [])))

    def _finalize(self):
        self.F = np.array([t[0] for t in self.tris], dtype=np.float64)  # (m,3,3)
        self.N = np.array([t[1] for t in self.tris], dtype=np.float64)
        self.C = np.array([t[2] for t in self.tris], dtype=np.float64)
        self.names = [t[3] for t in self.tris]
        self.hidden = np.zeros(len(self.F), dtype=bool)
        for it in self.inter:
            if "enemy" in it["tags"]:
                it["base"] = self.F[it["slice"][0]:it["slice"][1]].copy()

def ground_at(solids, x, y, z):
    best = -math.inf
    for lo, hi in solids:
        if lo[0] - 0.3 <= x <= hi[0] + 0.3 and lo[2] - 0.3 <= z <= hi[2] + 0.3:
            if hi[1] <= y + 0.6 and hi[1] > best:
                best = hi[1]
    return best

def run():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", default=".")
    ap.add_argument("--width", type=int, default=1280)
    ap.add_argument("--height", type=int, default=720)
    ap.add_argument("--fov", type=float, default=62.0)
    ap.add_argument("--frames", type=int, default=0, help="auto-exit after N frames (self-test)")
    ap.add_argument("--dummy", action="store_true", help="SDL dummy video driver")
    a = ap.parse_args()
    if a.dummy:
        os.environ["SDL_VIDEODRIVER"] = "dummy"
    project = Path(a.project).resolve()

    w = World(project)
    st, gp, env, ident = w.state, w.state.get("gameplay", {}), w.state.get("environment", {}), w.state.get("identity", {})
    phys = gp.get("physics", {})
    G = phys.get("gravity", 22); JUMPV = phys.get("jump_velocity", 8)
    RUN = phys.get("run_speed", 7); COYOTE = phys.get("coyote_time_s", 0.1)
    genre = gp.get("genre", "");
    mode = "3D"
    mv = str(ident.get("movement") or "").lower(); cam = str(ident.get("camera") or "").lower()
    if "platformer" in mv or "side" in cam or genre == "platformer_2_5d": mode = "2D5"
    elif "top_down" in cam or "isometric" in cam: mode = "TOP"

    pygame.init()
    screen = pygame.display.set_mode((a.width, a.height))
    pygame.display.set_caption("Litt Play - " + str(st.get("theme", "world")) + " [" + mode + "]")
    font = pygame.font.SysFont("consolas", 16)
    bigfont = pygame.font.SysFont("consolas", 42, bold=True)
    clock = pygame.time.Clock()

    spawn = np.array([0.0, 1.2, 4.0]); pos = spawn.copy(); vel = np.zeros(3)
    cam_yaw = math.pi; grounded = False; coyote = 0.0; buffer_t = 0.0
    score = 0; dead_until = 0.0; won = False
    light = np.array([0.45, 0.78, 0.32]); light = light / np.linalg.norm(light)
    sky = env.get("sky", {}).get("top_color") or [0.53, 0.72, 0.83]
    bg = tuple(int(c * 255) for c in sky)

    running = True; frame = 0; fps = 0.0
    while running:
        dt = min(clock.tick(60) / 1000.0, 0.05)
        frame += 1
        for e in pygame.event.get():
            if e.type == pygame.QUIT:
                running = False
            elif e.type == pygame.KEYDOWN:
                if e.key == pygame.K_ESCAPE: running = False
                elif e.key == pygame.K_SPACE: buffer_t = COYOTE + 0.02
                elif e.key == pygame.K_r: pos = spawn.copy(); vel[:] = 0
        keys = pygame.key.get_pressed()
        f = (keys[pygame.K_w] or keys[pygame.K_UP]) - (keys[pygame.K_s] or keys[pygame.K_DOWN])
        s = (keys[pygame.K_d] or keys[pygame.K_RIGHT]) - (keys[pygame.K_a] or keys[pygame.K_LEFT])
        if keys[pygame.K_q]: cam_yaw += 2.2 * dt
        if keys[pygame.K_e]: cam_yaw -= 2.2 * dt
        if mode == "2D5": dirx, dirz = float(f - s), 0.0
        elif mode == "TOP": dirx, dirz = float(s), float(f)
        else:
            fx, fz = -math.sin(cam_yaw), -math.cos(cam_yaw)
            dirx, dirz = fx * f - fz * s, fz * f + fx * s
        ln = math.hypot(dirx, dirz)
        vel[0] = dirx / ln * RUN if ln > 0 else 0.0
        vel[2] = dirz / ln * RUN if ln > 0 else 0.0
        coyote = COYOTE if grounded else max(0.0, coyote - dt)
        buffer_t = max(0.0, buffer_t - dt)
        if buffer_t > 0 and coyote > 0:
            vel[1] = JUMPV; coyote = 0.0; buffer_t = 0.0
        vel[1] -= G * dt
        pos = pos + vel * dt
        gy = ground_at(w.solids, pos[0], pos[1], pos[2])
        grounded = False
        if gy > -math.inf and pos[1] <= gy + 0.05 and vel[1] <= 0:
            pos[1] = gy; vel[1] = 0.0; grounded = True
        if pos[1] < -14:
            pos = spawn.copy(); vel[:] = 0

        aggro = gp.get("enemy_aggro_m", 6)
        now = time.time()
        for it in w.inter:
            if not it["alive"]: continue
            c = it["center"]; d = float(np.linalg.norm(c - pos))
            if "enemy" in it["tags"]:
                if d < aggro and d > 0.1:
                    step = (pos - c); step[1] = 0
                    step = step / (np.linalg.norm(step) + 1e-9) * 3.2 * dt
                    lo, hi = it["base"].min(axis=(0, 1)), it["base"].max(axis=(0, 1))
                    it["center"] = c + step
                    w.F[it["slice"][0]:it["slice"][1]] += step
                    it["base"] += step
                if d < 1.1:
                    pos = spawn.copy(); vel[:] = 0
                    dead_until = now + 0.5
            elif d < 1.6:
                tg = it["tags"]
                if tg & {"pickup", "score"}:
                    it["alive"] = False
                    w.hidden[it["slice"][0]:it["slice"][1]] = True
                    score += 25 if gp.get("scoring", {}).get("coins") else 10
                elif tg & {"goal", "win"}:
                    won = True
                elif "checkpoint" in tg:
                    spawn = c + np.array([0, 1.2, 0]); it["alive"] = False
                elif "poi" in tg:
                    it["alive"] = False

        # ---- camera ----
        if mode == "TOP": eye = pos + np.array([0, 34, 12]); look = pos + np.array([0, 0, 0.01])
        elif mode == "2D5": eye = pos + np.array([2, 6, 16]); look = pos + np.array([0, 1, 0])
        else:
            eye = pos + np.array([math.sin(cam_yaw) * 9, 4.5, math.cos(cam_yaw) * 9])
            look = pos + np.array([0, 1.4, 0])
        fwd = look - eye; fwd = fwd / np.linalg.norm(fwd)
        right = np.cross(fwd, [0, 1, 0]); right = right / np.linalg.norm(right)
        up = np.cross(right, fwd)
        rel = w.F.reshape(-1, 3) - eye
        cx = rel @ right; cy = rel @ up; cz = rel @ fwd
        aspect = a.width / a.height; focal = 1.0 / math.tan(math.radians(a.fov) / 2)
        okz = cz > 0.15
        sx = np.where(okz, a.width / 2 + cx * focal / aspect / np.maximum(cz, 1e-6) * a.width / 2, -9999)
        sy = np.where(okz, a.height / 2 - cy * focal / np.maximum(cz, 1e-6) * a.height / 2, -9999)
        P = np.stack([sx, sy], axis=1).reshape(-1, 3, 2)
        centers = w.F.mean(axis=1)
        tocam = eye - centers
        facing = np.einsum("ij,ij->i", w.N, tocam) > 0
        vis = okz.reshape(-1, 3).all(axis=1) & facing & (~w.hidden)
        depth = cz.reshape(-1, 3).mean(axis=1)
        order = np.argsort(-depth)
        shade = 0.55 + 0.45 * np.clip(np.einsum("ij,j->i", w.N, light), 0, 1)
        rgb = np.clip(w.C * shade[:, None] * 255, 0, 255).astype(int)

        screen.fill(bg)
        drw = pygame.draw
        for fi in order:
            if not vis[fi]: continue
            drw.polygon(screen, tuple(rgb[fi]), [tuple(P[fi][0]), tuple(P[fi][1]), tuple(P[fi][2])])

        hud1 = "%s | %s | score %d | fps %d" % (gp.get("genre", st.get("theme", "?")), mode, score, int(clock.get_fps()))
        hud2 = gp.get("objective", "explore")
        screen.blit(font.render(hud1, True, (255, 217, 122)), (12, 10))
        screen.blit(font.render(hud2, True, (191, 227, 242)), (12, 30))
        screen.blit(font.render("WASD move - Space jump - Q/E camera - R respawn - Esc quit", True, (140, 160, 170)), (12, a.height - 26))
        if won:
            t = bigfont.render("GOAL REACHED", True, (255, 217, 122))
            screen.blit(t, (a.width // 2 - t.get_width() // 2, a.height // 2 - 40))
        pygame.display.flip()
        if a.frames and frame >= a.frames:
            break
    pygame.quit()
    print("[native] rendered %d frames | %d tris | %d solids | %d interactives" % (frame, len(w.F), len(w.solids), len(w.inter)))

if __name__ == "__main__":
    run()