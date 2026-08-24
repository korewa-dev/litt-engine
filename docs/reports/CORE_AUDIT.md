# CORE_AUDIT.md — Adversarial review of `native/` C/C++ core

Scope: `native/littcore/litt_json.{c,h}`, `littcore/litt_obj.{c,h}`, `littcore/litt_world.{c,h}`, `native/littcli.c`, `native/tests.c`, `native/littview.cpp`.
Method: full read of all files; contract cross-check against reference `src/gameplay.rs` and against what `template/tools/worldgen/` actually emits; one empirical probe compiled from the real `litt_obj.c` into `%TEMP%` (no repo file modified).
Date: current session.

**VERDICT: NEEDS FIXES — 1 CRITICAL, 3 MAJOR, 8 MINOR, 12 NIT.**

---

## CRITICAL

### C1. NULL-pointer dereference in OBJ line splitter — `native/littcore/litt_obj.c:116`
- **What's wrong:** `*nl ? (*nl = 0) : 0;` evaluates the condition `*nl` unconditionally. When `memchr(line, '\n', ...)` returns NULL — i.e. the last line of the file has no trailing newline — this dereferences a null pointer.
- **Why it matters:** Confirmed empirically: a probe compiled from the real `litt_obj.c` segfaults (`exit=-1073741819`, 0xC0000005) on an OBJ ending `f 1 2 3<EOF>` and loads fine once a trailing `\n` is added. Any model file saved without a final newline crashes every consumer: `littcli validate`, `litt view`, and any host app using littcore. The shipped tests never hit it because their fixture strings all end with `\n`.
- **Fix:** Replace line 116 with:
  ```c
  if (nl) *nl = '\0';
  ```

## MAJOR

### M1. Mode-resolution substrings diverge from the runtime contract — `native/littcore/litt_world.c:43`
- **What's wrong:** Code resolves 2D5 via `strstr(movement, "2_5d")`. The reference (`src/gameplay.rs:310-318`) is `movement.contains("platformer") || camera.contains("side")`. Worldgen emits `"platformer_movement"`, `"parkour_movement"`, `"free_roam_movement"`, … — never `"2_5d"`.
- **Why it matters:** A generated platformer whose camera string contains neither `side` nor `top_down`/`isometric` resolves to **Orbit3D in C but Side2D5 in Rust**: wrong input mapping (W strafes instead of jumping semantics) and a wrong `"mode"` field in `littcli validate`'s machine-readable JSON. `tests.c:73` enshrines the bug by testing `"side_scrolling_2_5d"`, a value nothing produces.
- **Fix:** Match the reference exactly:
  ```c
  if (strstr(movement, "platformer") || strstr(camera, "side")) c->mode = LV_MODE_2D5;
  else if (strstr(camera, "top_down") || strstr(camera, "isometric")) c->mode = LV_MODE_TOP;
  else c->mode = LV_MODE_3D;
  ```
  and change the test vector to `{"identity":{"movement":"platformer_movement"}}`.

### M2. Extra movement-based TOP branch absent from the reference — `native/littcore/litt_world.c:45-47`
- **What's wrong:** C additionally maps `movement` containing `top_down`/`isometric` to TOP; Rust checks only the *camera* string.
- **Why it matters:** A state like `{movement:"top_down_walk", camera:"third_person"}` is Orbit3D in the reference but TOP in the C port — silent behavioral divergence for hand-authored worlds.
- **Fix:** Delete the two movement terms from the TOP check (see M1's corrected code).

### M3. Astral-plane (\u10000+) escapes corrupt strings — `native/littcore/litt_json.c:58-68, 95-102`
- **What's wrong:** `utf8_put` has no branch for cp ≥ 0x10000. A valid surrogate pair (`\uD83D\uDE00` → U+1F600) falls into the 3-byte path and emits lead byte `0xE0 | (cp>>12)` ∈ [0xF0,0xFF] followed by wrong continuations — invalid UTF-8, silently. Lone surrogates are also encoded raw, and a high surrogate followed by a non-`\u` char miscombines via unsigned underflow (`lo - 0xDC00` wraps), producing arbitrary wrong codepoints.
- **Why it matters:** Legal JSON round-trips wrongly: names/objectives containing emoji or any non-BMP character are corrupted in `cfg.objective`, entity names, tags. Tag lookups (`strcmp`) could silently miss. The Rust contract stores correct UTF-8. Memory-safe (max 3 bytes written vs 8-byte headroom) but a correctness failure on valid input, precisely in the escapes+surrogates area.
- **Fix:** Add the 4-byte form to `utf8_put`:
  ```c
  else if (cp < 0x10000) { /* existing 3-byte path */ }
  else {
      b[(*len)++] = (char)(0xF0 | (cp >> 18));
      b[(*len)++] = (char)(0x80 | ((cp >> 12) & 0x3F));
      b[(*len)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
      b[(*len)++] = (char)(0x80 | (cp & 0x3F));
  }
  ```
  Keep the growth check as `len + 8 > cap` (still covers 4 bytes). Reject/replace unpaired surrogates with U+FFFD instead of encoding them.

## MINOR

### m1. Explicit `jump_buffer_s <= 0` treated as absent — `native/littcore/litt_world.c:87,110`
- **Wrong:** `jb > 0 ? jb : coyote + 0.02f` uses a `-1` sentinel, so an explicit `0` becomes `coyote + 0.02`. Reference (`gameplay.rs:276-280`) uses any present value verbatim — `0` means "buffering disabled".
- **Matters:** Worlds that intentionally disable jump buffering get buffering re-enabled; physics diverges from the documented default relationship only when authors choose edge values.
- **Fix:** Test presence, not sign: `const LvJson *jbv = lvj_get(ph, "jump_buffer_s"); out->buffer = jbv ? (float)lvj_num(jbv, out->buffer) : out->coyote + 0.02f;`

### m2. Truncated JSON literals accepted — `native/littcore/litt_json.c:181-186`
- **Wrong:** `strncmp(p->s + p->i, "true", min(n-i,4))` matches `"tru"`, `"fals"`, `"nul"` at end-of-input, then advances `p->i` past `n`. `lvj_parse("tru")` returns `true`.
- **Matters:** Invalid documents parse successfully; downstream config silently takes default-ish values instead of failing validation.
- **Fix:** Require full length: `if (p->n - p->i >= 4 && !memcmp(p->s + p->i, "true", 4)) …` etc.

### m3. Lenient number grammar + silent truncation of long tokens — `native/littcore/litt_json.c:189-202`
- **Wrong:** Scan loop accepts leading `+`, bare `.5`, trailing `1.`, and strtod's hex extension `0x10` — none legal JSON. Tokens ≥ 64 chars are cut into `tmp[64]`; since the cut makes `*end == '\0'`, the truncated prefix is accepted as the value with no error.
- **Matters:** Malformed configs parse "successfully" with surprising numbers; a >63-digit literal yields a completely different magnitude silently.
- **Fix:** Validate with a strict manual scan (`-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?`) before strtod, and reject any token with `len >= sizeof(tmp)`.

### m4. Unchecked allocations crash on OOM — `litt_json.c:78`; `litt_obj.c:14,26,44,95-96`; `litt_world.c:122,168`; `littcli.c:21`
- **Wrong:** `fv_push`/`iv_push`/`rmap_put`/FLUSH's realloc and `ent_push`'s realloc assign directly without checking; `read_file`/`lv_session_create` malloc unchecked; `parse_str_raw`'s realloc also loses the old pointer on failure.
- **Matters:** A large or hostile OBJ/LSCN turns an allocation failure into a NULL write → crash instead of a clean error return (`solid_push` at `litt_world.c:127-133` already models the right pattern).
- **Fix:** Check every result; on failure free partial state and propagate the error code (loader returns nonzero, CLI prints a diagnostic).

### m5. Negative scale inverts solid AABBs — `native/littcore/litt_world.c:146-151`
- **Wrong:** `lo = bmin*scale + pos; hi = bmax*scale + pos;` — with negative scale lo>hi, so the union produces `min > max` per axis.
- **Matters:** A mirrored node yields an inside-out solid: `ground_at`/`collide_walls` tests become unsatisfiable, so platforms vanish or walls behave erratically. Same latent issue in the viewer's bounds union.
- **Fix:** Compute both products then order them: `float p0 = me->bmin[k]*scale + pos[k], p1 = me->bmax[k]*scale + pos[k]; float lo = p0<p1?p0:p1, hi = p0<p1?p1:p0;`

### m6. Viewer model cache leaks every loaded mesh — `native/littview.cpp:215,239,243-246`
- **Wrong:** `std::unordered_map<std::string, LvModel>` stores POD structs owning malloc'd `verts`/`idx`; no destructor runs and `lv_model_free` is never called when `load()` returns.
- **Matters:** All model geometry leaks per `load()` call. Currently bounded (one load per process in render/window/bench modes) but grows unbounded the moment hot-reload calls `load()` again.
- **Fix:** Before returning from `load()` (both success and early-return paths), iterate the cache and `lv_model_free` each value; or store RAII handles (`std::unique_ptr<LvModel,…>` with a deleter calling `lv_model_free`).

### m7. Window mode cannot be closed with X — `native/littview.cpp:521-561`
- **Wrong:** Class wndproc is `DefWindowProcA`; WM_DESTROY never posts WM_QUIT, and the loop only exits on WM_QUIT/VK_ESCAPE. Clicking X destroys the window but the loop keeps rendering/blitting forever.
- **Matters:** Invisible zombie process at ~100% CPU after the user closes the window; Esc is undiscoverable.
- **Fix:** Give the class a real WndProc handling `WM_DESTROY → PostQuitMessage(0)`, and/or guard each frame with `if (!IsWindow(hwnd)) break;`.

### m8. Float→int casts of unclamped projected coordinates — `native/littview.cpp:452-455`
- **Wrong:** `(int)fmaxf(0, floorf(min_hx))` clamps only the lower side after conversion intent; a projected coordinate beyond INT_MAX converts with UB before/outside the clamp (reachable only with `cw` near the 0.05 gate plus huge world coords — theoretical for generated content).
- **Matters:** UB and potentially absurd bbox loops on adversarial lscn/OBJ data.
- **Fix:** Clamp in the float domain first, e.g. `float fx = fminf(fmaxf(floorf(hmin), 0.f), (float)(fb.w - 1)); int minx = (int)fx;` (same for max/y).

## NIT / polish

### n1. No top-left fill rule — `native/littview.cpp:483` *(answer to the rasterizer question)*
Shared-edge pixels satisfy `w >= 0` (w exactly 0) in **both** adjacent triangles → drawn twice. For opaque single-color-per-tri fills under the z-test this is acceptable: the second writer wins only if strictly closer. Residual risk: hairline seams/sparkle where the two triangles interpolate slightly different FP depths along the shared diagonal. Fix only if seams appear: add a top-left bias (`w > 0 || (w == 0 && edge_is_top_left)`).

### n2. Whole-triangle near rejection — `native/littview.cpp:422-429`
Any vertex with `sw[k] < 0.05f` drops the entire triangle, so large ground/wall triangles pop as the camera approaches. Acceptable for a preview tool; proper fix is near-plane interpolation/clipping.

### n3. Normals parsed but discarded; remap contradicts header — `native/littcore/litt_obj.c:31,126-130,160-166`
`vn` lines feed the `gn` pool which is never read; the remap keys on gpos with `gnorm` hardcoded −1 despite the header claiming "(pos,norm) pair". Harmless while the viewer flat-shades; will weld vertices wrongly if smooth normals are ever needed. Fix: drop `gn`, or key the remap on `(gpos,gnorm)`.

### n4. OBJ robustness gaps — `native/littcore/litt_obj.c:149-165`
Negative indices (`f -1 -2 -3`, legal OBJ) silently abort the face; tab separators between corners unsupported; `(int)tri[t]*3 + k` can overflow signed int for indices ≳715M before the `gp.n` guard rejects. Fix: support negative wraparound or reject explicitly; skip tabs; compute offsets in unsigned/size_t.

### n5. Respawn leaves stale jump timers — `native/littcore/litt_world.c:397-405`
Respawn resets position/velocity but not `grounded`/`coyote_t`/`buf_t`; a jump buffered just before death fires instantly on the respawn frame. Fix: zero `coyote_t`/`buf_t`/`grounded` when teleporting.

### n6. Tier matching case sensitivity — `native/littcore/litt_world.c:242-249`
C matches names case-insensitively; `gameplay.rs:458-463` uses case-sensitive `starts_with`/`contains`. Substring/prefix/model-tag logic otherwise matches exactly. Pick one convention (prefer matching Rust) and note it.

### n7. Tests don't cover what they claim — `native/tests.c:114-116` (+ `:19-23`)
The "heavy-model enemy reads as boss tier" assertion actually passes via the name-substring branch ("Boss_X"); the knight/brute-model → elite rule is untested. `write_file` ignores fopen failure → NULL fputs on a read-only cwd. Fix: add a `mob_grunt` with `model:knight` expecting ELITE; check fopen.

### n8. Validation gate missing solids — `native/littcli.c:72`
`ok` requires `interactives > 0` but not `solids > 0`, while AGENTS.md's native-validation gate expects both for platformers. Consider `s.solid_count > 0` at least when mode == Side2D5/TOP.

### n9. POSIX-only API despite C11 claim — `native/littcore/litt_world.c:242,244`
`strncasecmp` is POSIX, not C11; fine under MinGW/gcc (current build.bat) but breaks MSVC. Use a tiny local lowercase compare or `_strnicmp` under `_MSC_VER`.

### n10. Trailing garbage accepted after root value — `native/littcore/litt_json.c:210-215`
`lvj_parse("{} junk")` succeeds. Add a post-parse `skip_ws; require i==n` check if callers should reject malformed files (config validation would benefit).

### n11. Only `scale[0]` honored — `native/littcore/litt_world.c:190-192`, `native/littview.cpp:226-227`
Non-uniform `[sx,sy,sz]` silently degrades to sx for solids and render. Generators emit uniform scales today; either honor all axes or document the restriction.

### n12. Non-finite config values flow through — `litt_json.c` strtod + `litt_world.c` consumers
`strtod` overflow ("gravity": 1e999) yields inf → positions go NaN within ticks. `littcli`'s `nan_check` catches the NaN end-state (good); littview would render NaN colors (no crash). Fix: reject non-finite numbers in `lv_config_from_state`.

---

## Verified correct (hunt-list items that came back clean)

- **JSON string buffer safety:** growth check `len + 8 > cap` precedes every character; worst-case write is 3 bytes (`\u` escape) plus final NUL — overflow impossible. `hex4` bounds-checked; recursion depth cap 128 with balanced decrements on every exit path; `push_item`'s partial-failure ordering (count incremented last) prevents double-frees.
- **OBJ index remapping & per-mesh bounds:** remap reset per FLUSH; ownership handoff nulls `cv/ci` (no double-free at cleanup); out-of-range global indices substitute 0 rather than overreading; `mesh_bounds`' `i + 2 < vn*3` iterates exactly all triplets over referenced vertices only.
- **Rasterizer delta math (priority question):** verified algebraically exact. `st0x/st0y/st1x/st1y` are precisely ∂λ₀/∂x, ∂λ₀/∂y, ∂λ₁/∂x, ∂λ₁/∂y of the signed-area-normalized edge functions, and `r0/r1` are the true barycentric values at the first pixel center, so the incremental scheme equals per-pixel evaluation modulo ~1e-6 FP accumulation drift. Signed `inv_area` normalizes both windings. The floor/ceil bbox is a superset of candidate pixel centers (≤1 px over-scan, no missed pixels), and the 0/w−1 clamps keep every row/column write in-bounds. Screen-space linear interpolation of z/w is exact for planar triangles; "smaller = closer" matches the `persp()` sign conventions. Remaining caveat is n1 (double-drawn shared edges).
- `littcli` NaN guard and argument parsing; BMP writer layout/row padding for sane dimensions; `look_at` degeneracy avoided by the 24°/55° elevation choices; `vnorm` epsilon clamp prevents div-by-zero NaN.

## Verdict

**VERDICT: NEEDS FIXES** (C1 must be fixed before any real-world asset pass; M1–M3 should land with it so the native port matches the documented runtime contract.)
