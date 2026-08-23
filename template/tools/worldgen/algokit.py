#!/usr/bin/env python3
"""ALGOKIT - executable implementations of every algorithm in genre_algorithms.md.

Before this module the encyclopedia documented formulas; agents had to
re-implement them. Now they import them. All functions are deterministic
(seeded Rng from worldkit) and dependency-free (stdlib only).

Contents:
  Vec2 / Vec3            - minimal vector math (add sub scale dot cross len norm rot)
  bsp_partition          - roguelike room partitioning
  cellular_cave          - cellular automata cave carving
  astar                  - A* shortest path on tile grids
  bfs_flow_field         - tower-defense distance fields from goal cells
  bresenham_line         - integer line (LOS checks, corridor carving)
  has_line_of_sight      - grid LOS using bresenham
  poisson_disc_points    - even scatter (Bridson), placement without overlap
  fisher_yates           - unbiased shuffle (card decks, spawn tables)
  solve_jump_arc         - platformer feasibility (height, airtime, range)
  catmull_rom_2d/3d      - splines for tracks, roads, camera paths
"""
import heapq
import math
from random import Random

# ------------------------------------------------------------------ vectors
class Vec2:
    __slots__ = ("x", "y")
    def __init__(self, x, y): self.x, self.y = float(x), float(y)
    def __add__(self, o): return Vec2(self.x + o.x, self.y + o.y)
    def __sub__(self, o): return Vec2(self.x - o.x, self.y - o.y)
    def __mul__(self, s): return Vec2(self.x * s, self.y * s)
    def dot(self, o): return self.x * o.x + self.y * o.y
    def length(self): return math.hypot(self.x, self.y)
    def norm(self):
        l = self.length()
        return Vec2(self.x / l, self.y / l) if l > 1e-12 else Vec2(0, 0)
    def rotated(self, rad):
        c, s = math.cos(rad), math.sin(rad)
        return Vec2(self.x * c - self.y * s, self.x * s + self.y * c)
    def perp(self): return Vec2(-self.y, self.x)
    def as_tuple(self): return (round(self.x, 4), round(self.y, 4))
    def __repr__(self): return "Vec2(%g, %g)" % (self.x, self.y)

class Vec3(Vec2):
    __slots__ = ("z",)
    def __init__(self, x, y, z): super().__init__(x, y); self.z = float(z)
    def __add__(self, o): return Vec3(self.x + o.x, self.y + o.y, self.z + o.z)
    def __sub__(self, o): return Vec3(self.x - o.x, self.y - o.y, self.z - o.z)
    def __mul__(self, s): return Vec3(self.x * s, self.y * s, self.z * s)
    def dot(self, o): return self.x * o.x + self.y * o.y + self.z * o.z
    def cross(self, o):
        return Vec3(self.y * o.z - self.z * o.y, self.z * o.x - self.x * o.z, self.x * o.y - self.y * o.x)
    def length(self): return math.sqrt(self.dot(self))
    def norm(self):
        l = self.length()
        return Vec3(self.x / l, self.y / l, self.z / l) if l > 1e-12 else Vec3(0, 0, 0)
    def __repr__(self): return "Vec3(%g, %g, %g)" % (self.x, self.y, self.z)

# --------------------------------------------------------------- partitions
def bsp_partition(x, y, w, h, depth, rng, min_size=6.0):
    """Split rect into leaf rooms. Returns [(x, y, w, h), ...]."""
    if depth <= 0 or (w < min_size * 2 and h < min_size * 2):
        return [(x, y, w, h)]
    horizontal = rng.uniform() > 0.5
    if w < min_size * 2: horizontal = True   # width too small - MUST cut height
    elif h < min_size * 2: horizontal = False # height too small - MUST cut width
    if horizontal:
        cut = rng.uniform(min_size, max(min_size + 0.01, h - min_size))
        return (bsp_partition(x, y, w, cut, depth - 1, rng, min_size)
                + bsp_partition(x, y + cut, w, h - cut, depth - 1, rng, min_size))
    cut = rng.uniform(min_size, max(min_size + 0.01, w - min_size))
    return (bsp_partition(x, y, cut, h, depth - 1, rng, min_size)
            + bsp_partition(x + cut, y, w - cut, h, depth - 1, rng, min_size))

# ------------------------------------------------------------------- caves
def cellular_cave(w, h, rng, fill_prob=0.45, iterations=4, wall_neighbors=5):
    """Cellular automata cave. Returns list of strings, "#" wall, "." floor."""
    grid = [["#" if rng.uniform() < fill_prob else "." for _ in range(w)] for _ in range(h)]
    for _ in range(iterations):
        nxt = [row[:] for row in grid]
        for yy in range(h):
            for xx in range(w):
                walls = 0
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx == 0 and dy == 0: continue
                        ny, nx = yy + dy, xx + dx
                        if ny < 0 or nx < 0 or ny >= h or nx >= w or grid[ny][nx] == "#": walls += 1
                nxt[yy][xx] = "#" if walls >= wall_neighbors else "."
        grid = nxt
    for x2 in range(w): grid[0][x2] = grid[h - 1][x2] = "#"
    for y2 in range(h): grid[y2][0] = grid[y2][w - 1] = "#"
    return ["".join(row) for row in grid]

# ------------------------------------------------------------------ paths
def astar(walkable, start, goal):
    """A* over dict/set of walkable (x, y) cells. Manhattan heuristic.
    Returns list of cells start..goal, or None."""
    if start not in walkable or goal not in walkable: return None
    openq = [(0, start)]; came = {}; g = {start: 0}
    while openq:
        _, cur = heapq.heappop(openq)
        if cur == goal:
            path = [cur]
            while cur in came: cur = came[cur]; path.append(cur)
            return path[::-1]
        cx, cy = cur
        for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
            nb = (nx, ny)
            if nb not in walkable: continue
            ng = g[cur] + 1
            if ng < g.get(nb, 1 << 30):
                g[nb] = ng; came[nb] = cur
                hh = abs(nx - goal[0]) + abs(ny - goal[1])
                heapq.heappush(openq, (ng + hh, nb))
    return None

def bfs_flow_field(walkable, goals):
    """Distance-to-nearest-goal map via BFS (tower defense). Returns dict cell->dist."""
    dist = {}
    q = []
    for gl in goals:
        if gl in walkable: dist[gl] = 0; q.append(gl)
    head = 0
    while head < len(q):
        cur = q[head]; head += 1
        cx, cy = cur
        for nb in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
            if nb in walkable and nb not in dist:
                dist[nb] = dist[cur] + 1; q.append(nb)
    return dist

def bresenham_line(a, b):
    """Integer cells from a to b inclusive."""
    (x0, y0), (x1, y1) = a, b
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    out = []
    while True:
        out.append((x0, y0))
        if x0 == x1 and y0 == y1: break
        e2 = 2 * err
        if e2 >= dy: err += dy; x0 += sx
        if e2 <= dx: err += dx; y0 += sy
    return out

def has_line_of_sight(blocked, a, b):
    """True if straight cells between a and b hit no blocked cell."""
    return not any(c in blocked for c in bresenham_line(a, b)[1:-1])

# ---------------------------------------------------------------- sampling
def poisson_disc_points(width, height, min_dist, rng, k=20):
    """Bridson sampling: even coverage, no two points closer than min_dist.
    Cell size equals min_dist so any rejecting-distance neighbor is guaranteed
    to lie within the scanned 3x3 window."""
    cell = min_dist
    gw, gh = int(math.ceil(width / cell)), int(math.ceil(height / cell))
    grid = [None] * (gw * gh)
    pts = []; active = []
    p0 = Vec2(rng.uniform(0, width), rng.uniform(0, height))
    def grid_put(pt):
        gx, gy = int(pt.x / cell), int(pt.y / cell)
        if 0 <= gx < gw and 0 <= gy < gh: grid[gy * gw + gx] = pt
    grid_put(p0); pts.append(p0); active.append(p0)
    while active:
        i = int(rng.next_u32() % len(active)); base = active[i]
        placed = False
        for _ in range(k):
            ang = rng.uniform(0, 2 * math.pi)
            rad = rng.uniform(min_dist, 2 * min_dist)
            cand = base + Vec2(math.cos(ang), math.sin(ang)).__mul__(rad)
            if not (0 <= cand.x < width and 0 <= cand.y < height): continue
            ok = True
            gx, gy = int(cand.x / cell), int(cand.y / cell)
            for oy in (-1, 0, 1):
                for ox in (-1, 0, 1):
                    nx_, ny_ = gx + ox, gy + oy
                    if 0 <= nx_ < gw and 0 <= ny_ < gh:
                        nb = grid[ny_ * gw + nx_]
                        if nb and (cand - nb).length() < min_dist: ok = False; break
                if not ok: break
            if ok:
                grid_put(cand); pts.append(cand); active.append(cand); placed = True; break
        if not placed: active.pop(i)
    return pts

def fisher_yates(items, rng):
    """Unbiased shuffle; returns new list."""
    out = list(items)
    for i in range(len(out) - 1, 0, -1):
        j = rng.next_u32() % (i + 1)
        out[i], out[j] = out[j], out[i]
    return out

# ------------------------------------------------------------------- jumps
def solve_jump_arc(jump_velocity, gravity, run_speed):
    """Kinematics: peak height v^2/(2g), airtime 2v/g, horizontal range.
    Use range to cap generated gap widths."""
    height = jump_velocity ** 2 / (2.0 * gravity)
    airtime = 2.0 * jump_velocity / gravity
    return {"peak_height_m": round(height, 3), "airtime_s": round(airtime, 3),
            "max_range_m": round(run_speed * airtime, 3)}

def can_clear_gap(gap_width, jump_velocity, gravity, run_speed):
    return gap_width <= run_speed * (2.0 * jump_velocity / gravity)

# ------------------------------------------------------------------ spline
def catmull_rom_2d(p0, p1, p2, p3, t):
    """Catmull-Rom through p1..p2 segment; points are Vec2."""
    t2, t3 = t * t, t * t * t
    def cm(a, b, c, d):
        return 0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3)
    return Vec2(cm(p0.x, p1.x, p2.x, p3.x), cm(p0.y, p1.y, p2.y, p3.y))

def closed_loop_samples(control_points, samples_per_segment):
    """Sample a closed Catmull-Rom loop through control points."""
    n = len(control_points); out = []
    for i in range(n):
        p0, p1 = control_points[(i - 1) % n], control_points[i]
        p2, p3 = control_points[(i + 1) % n], control_points[(i + 2) % n]
        for s in range(samples_per_segment):
            out.append(catmull_rom_2d(p0, p1, p2, p3, s / float(samples_per_segment)))
    return out