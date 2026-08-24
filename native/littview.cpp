// littview.cpp - Litt Stage-2 renderer front-end (C++17, no dependencies
// beyond libc/gdi32). Consumes littcore: loads a generated game's
// world_state + lscn + OBJ part models, bakes the engine's environment
// lighting (sun diffuse + hemispheric ambient + distance haze), and renders
// an orbit-camera frame through a depth-buffered software rasterizer.
//
//   littview render  <dir> [--yaw d] [--hgt f] [--w W] [--h H] [--out f.bmp]
//   littview window  <dir>                (Win32: arrows orbit, Esc quits)
//   littview selftest                     (offscreen pixel assertions)
//
// This is the seed of the Stage-2 C++ front-end: the DIB blit becomes a
// Vulkan swapchain present later without touching the scene/bake pipeline.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include "littcore/litt_world.h"
#include "littcore/litt_json.h"
#include "littcore/litt_obj.h"

// ------------------------------------------------------------------ math
struct V3 {
    float x, y, z;
};
static V3 vsub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static V3 vcross(V3 a, V3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static float vdot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 vnorm(V3 a) {
    float l = sqrtf(vdot(a, a));
    if (l < 1e-6f) l = 1e-6f;
    return {a.x / l, a.y / l, a.z / l};
}

static void mat_mul(const float A[16], const float B[16], float O[16]) {
    float t[16];
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += A[k * 4 + r] * B[c * 4 + k];
            t[c * 4 + r] = s;
        }
    memcpy(O, t, sizeof(t));
}

// Column-major, mirrors src/studio.rs perspective()/look_at()
static void persp(float fovy_deg, float aspect, float n, float f, float M[16]) {
    float t = 1.0f / tanf(fovy_deg * 3.14159265f / 180.0f);
    float nf = 1.0f / (n - f);
    M[0] = t / aspect; M[1] = M[2] = M[3] = 0;
    M[4] = 0; M[5] = t; M[6] = M[7] = 0;
    M[8] = M[9] = 0; M[10] = (f + n) * nf; M[11] = -1;
    M[12] = M[13] = 0; M[14] = 2 * f * n * nf; M[15] = 0;
}
static void look_at(V3 eye, V3 at, float M[16]) {
    V3 up = {0, 1, 0};
    V3 z = vnorm(vsub(eye, at));
    V3 x = vnorm(vcross(up, z));
    V3 y = vcross(z, x);
    M[0] = x.x; M[1] = y.x; M[2] = z.x; M[3] = 0;
    M[4] = x.y; M[5] = y.y; M[6] = z.y; M[7] = 0;
    M[8] = x.z; M[9] = y.z; M[10] = z.z; M[11] = 0;
    M[12] = -vdot(x, eye); M[13] = -vdot(y, eye); M[14] = -vdot(z, eye); M[15] = 1;
}

static float quat_yaw(const float q[4]) {
    return atan2f(2.0f * (q[1] * q[3] + q[0] * q[2]),
                  1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2]));
}

// ------------------------------------------------------------ framebuffer
struct FB {
    int w = 0, h = 0;
    std::vector<unsigned> px;   // 0xAARRGGBB
    std::vector<float> depth;   // view-space Z (smaller = closer)
    void resize(int W, int H) {
        w = W; h = H;
        px.assign((size_t)W * H, 0xFF20180F); // warm dark clear
        depth.assign((size_t)W * H, 1e30f);
    }
    void clear(void) {
        std::fill(px.begin(), px.end(), 0xFF20180Fu);
        std::fill(depth.begin(), depth.end(), 1e30f);
    }
};

static bool write_bmp(const char *path, const FB &fb) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    int row = (fb.w * 3 + 3) & ~3;
    unsigned data = 54u + (unsigned)row * fb.h;
    unsigned char hdr[54] = {'B', 'M'};
    memcpy(hdr + 2, &data, 4);
    unsigned off = 54, hsz = 40;
    short planes = 1, bpp = 24;
    memset(hdr + 6, 0, 8); /* reserved1/2 */
    memcpy(hdr + 10, &off, 4); memcpy(hdr + 14, &hsz, 4);
    memcpy(hdr + 18, &fb.w, 4); memcpy(hdr + 22, &fb.h, 4);
    memcpy(hdr + 26, &planes, 2); memcpy(hdr + 28, &bpp, 2);
    fwrite(hdr, 1, 54, f);
    std::vector<unsigned char> line((size_t)row);
    for (int y = fb.h - 1; y >= 0; y--) {
        for (int x = 0; x < fb.w; x++) {
            unsigned p = fb.px[(size_t)y * fb.w + x];
            line[x * 3 + 0] = (unsigned char)(p & 255);         // B
            line[x * 3 + 1] = (unsigned char)((p >> 8) & 255);  // G
            line[x * 3 + 2] = (unsigned char)((p >> 16) & 255); // R
        }
        fwrite(line.data(), 1, (size_t)row, f);
    }
    fclose(f);
    return true;
}

// ------------------------------------------------------- env + bake light
struct EnvLight {
    float sky[3] = {0.35f, 0.55f, 0.9f};
    float sun_el = 50, sun_az = 135, intensity = 1;
};
static EnvLight env_from_state(const char *state_text) {
    EnvLight e;
    LvJson *r = lvj_parse(state_text);
    if (!r) return e;
    const LvJson *env = lvj_get(r, "environment");
    if (env) {
        float c[3];
        if (lvj_arr_f3(lvj_get(lvj_get(env, "sky"), "top_color"), c)) {
            e.sky[0] = c[0]; e.sky[1] = c[1]; e.sky[2] = c[2];
        }
        const LvJson *sun = lvj_get(env, "sun");
        if (sun) {
            e.sun_el = (float)lvj_num(lvj_get(sun, "elevation_deg"), e.sun_el);
            e.sun_az = (float)lvj_num(lvj_get(sun, "azimuth_deg"), e.sun_az);
        }
        e.intensity =
            (float)lvj_num(lvj_get(lvj_get(env, "lighting"), "global_light_intensity"),
                           e.intensity);
    }
    lvj_free(r);
    return e;
}

struct Tri {
    V3 p[3];
    float col[3]; // baked rgb
};

struct Scene {
    std::vector<Tri> tris;
    float centre[3] = {0, 0, 0};
    float radius = 1;   // full bounds radius (haze scale)
    float fit_r = 10;   // robust framing radius (percentile of centroids)
    float auto_yaw = 0.7f;
    float bounds_min[3] = {0, 0, 0}, bounds_max[3] = {0, 0, 0};
    long missing = 0;

    // Build from a game dir: every visible node with model: contributes its
    // transformed triangles, shaded exactly like the engine's bake.
    bool load(const char *dir) {
        char path[1024], models[1024];
        snprintf(path, sizeof(path), "%s/world_state.json", dir);
        FILE *f = fopen(path, "rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::string state((size_t)sz + 1, 0);
        size_t rd = fread(state.data(), 1, (size_t)sz, f);
        state[rd] = 0;
        fclose(f);

        snprintf(models, sizeof(models), "%s/assets/models", dir);
        EnvLight env = env_from_state(state.c_str());

        snprintf(path, sizeof(path), "%s/assets/scenes/world.lscn.json", dir);
        f = fopen(path, "rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::string scn((size_t)sz + 1, 0);
        rd = fread(scn.data(), 1, (size_t)sz, f);
        scn[rd] = 0;
        fclose(f);

        LvJson *root = lvj_parse(scn.c_str());
        if (!root) return false;
        const LvJson *nodes = lvj_get(root, "nodes");
        if (!nodes) { lvj_free(root); return false; }

        // material palette per tag family (matches engine materials)
        auto tint_of = [&](const LvJson *tags, float out[3]) {
            out[0] = out[1] = out[2] = 0.62f;
            if (has_tag(tags, "enemy")) { out[0] = 0.78f; out[1] = 0.26f; out[2] = 0.24f; }
            else if (has_tag(tags, "hazard")) { out[0] = 0.85f; out[1] = 0.45f; out[2] = 0.12f; }
            else if (has_tag(tags, "checkpoint")) { out[0] = 0.95f; out[1] = 0.75f; out[2] = 0.25f; }
            else if (has_tag(tags, "scoring") || has_tag(tags, "pickup") ||
                     has_tag(tags, "token") || has_tag(tags, "objective")) {
                out[0] = 0.98f; out[1] = 0.82f; out[2] = 0.20f;
            } else if (has_tag(tags, "goal") || has_tag(tags, "win")) {
                out[0] = 0.35f; out[1] = 0.85f; out[2] = 0.45f;
            }
        };

        // pass 1: gather transformed triangles with albedo*tint, track bounds
        struct Raw { V3 p[3], n; float alb[3]; };
        std::vector<Raw> raws;
        float mn[3] = {1e9f, 1e9f, 1e9f}, mx[3] = {-1e9f, -1e9f, -1e9f};
        // parsed models live for the whole render (no per-node reload)
        std::unordered_map<std::string, LvModel> cache;

        for (int i = 0; i < nodes->count; i++) {
            const LvJson *n = lvj_at(nodes, i);
            if (!n || !lvj_bool(lvj_get(n, "visible"), 1)) continue;
            const LvJson *tags = lvj_get(n, "tags");
            const char *mdl = model_of(tags);
            if (!mdl) continue;
            float pos[3] = {0, 0, 0}, scl = 1.0f, rot[4] = {0, 0, 0, 1};
            lvj_arr_f3(lvj_get(n, "position"), pos);
            const LvJson *sj = lvj_get(n, "scale");
            if (sj && sj->kind == LJ_ARR && sj->count >= 1)
                scl = (float)lvj_num(lvj_at(sj, 0), 1.0f);
            const LvJson *rj = lvj_get(n, "rotation");
            if (rj && rj->kind == LJ_ARR && rj->count >= 4)
                for (int k = 0; k < 4; k++) rot[k] = (float)lvj_num(lvj_at(rj, k), rot[k]);
            float yaw = quat_yaw(rot);

            std::string key(mdl);
            if (cache.find(key) == cache.end()) {
                char p2[1024];
                snprintf(p2, sizeof(p2), "%s/%s.obj", models, mdl);
                LvModel m;
                if (lv_obj_load(p2, &m)) {
                    cache[key] = LvModel{nullptr, 0};
                    missing++;
                    continue;
                }
                cache[key] = m;
            }
            const LvModel &m = cache[key];
            if (!m.meshes) { missing++; continue; }

            float tint[3];
            tint_of(tags, tint);
            float cy = cosf(yaw), sy = sinf(yaw);
            for (int mi = 0; mi < m.count; mi++) {
                const LvMesh *me = &m.meshes[mi];
                for (int t = 0; t + 2 < me->in; t += 3) {
                    Raw r;
                    for (int k = 0; k < 3; k++) {
                        unsigned ix = me->idx[t + k] * 3;
                        float x = me->verts[ix] * scl, y = me->verts[ix + 1] * scl,
                              z = me->verts[ix + 2] * scl;
                        r.p[k] = {pos[0] + x * cy + z * sy,
                                  pos[1] + y,
                                  pos[2] - x * sy + z * cy};
                    }
                    V3 e1 = vsub(r.p[1], r.p[0]), e2 = vsub(r.p[2], r.p[0]);
                    r.n = vnorm(vcross(e1, e2));
                    for (int a = 0; a < 3; a++) {
                        const V3 &q = r.p[a];
                        if (q.x < mn[0]) mn[0] = q.x;
                        if (q.x > mx[0]) mx[0] = q.x;
                        if (q.y < mn[1]) mn[1] = q.y;
                        if (q.y > mx[1]) mx[1] = q.y;
                        if (q.z < mn[2]) mn[2] = q.z;
                        if (q.z > mx[2]) mx[2] = q.z;
                    }
                    // per-mesh albedo from part height band (part shading)
                    float shade =
                        0.55f + 0.4f * fabsf(me->bmin[1]) / (fabsf(me->bmax[1]) + 1.f);
                    r.alb[0] = tint[0] * shade;
                    r.alb[1] = tint[1] * shade;
                    r.alb[2] = tint[2] * shade;
                    raws.push_back(r);
                }
            }
        }
        lvj_free(root);
        if (getenv("LITT_DEBUG"))
            fprintf(stderr,
                    "[dbg] raws=%zu bounds=[%.1f %.1f %.1f]..[%.1f %.1f %.1f]\n",
                    raws.size(), mn[0], mn[1], mn[2], mx[0], mx[1], mx[2]);
        if (raws.empty()) return false;

        centre[0] = (mn[0] + mx[0]) * .5f;
        centre[1] = (mn[1] + mx[1]) * .5f;
        centre[2] = (mn[2] + mx[2]) * .5f;
        radius = 0.5f * sqrtf(powf(mx[0] - mn[0], 2) + powf(mx[1] - mn[1], 2) +
                              powf(mx[2] - mn[2], 2));
        if (radius < 1e-4f) radius = 1;
        for (int a = 0; a < 3; a++) {
            bounds_min[a] = mn[a];
            bounds_max[a] = mx[a];
        }

        // robust framing radius: 88th-percentile triangle-centroid distance
        // measured from the real centre (keeps backdrop planes like ground
        // discs from zooming us out)
        {
            std::vector<float> ds;
            ds.reserve(raws.size());
            for (const Raw &r : raws) {
                float dx = (r.p[0].x + r.p[1].x + r.p[2].x) / 3 - centre[0];
                float dy = (r.p[0].y + r.p[1].y + r.p[2].y) / 3 - centre[1];
                float dz = (r.p[0].z + r.p[1].z + r.p[2].z) / 3 - centre[2];
                ds.push_back(sqrtf(dx * dx + dy * dy + dz * dz));
            }
            std::sort(ds.begin(), ds.end());
            size_t k = (size_t)(ds.size() * 0.88);
            if (k >= ds.size()) k = ds.size() - 1;
            fit_r = ds[k] * 1.15f + 2.0f;
            // arenas sitting on huge ground discs: never zoom past this
            if (fit_r > 42) fit_r = 42;
            if (fit_r < 6) fit_r = 6;
        }
        // corridors/platformers: frame from a DIAGONAL 40 deg off the long
        // axis - broadside shows only the thin cross-section, end-on hides
        // the length; the diagonal stacks depth AND width into frame
        float long_x = (mx[0] - mn[0] >= mx[2] - mn[2]);
        float base = long_x ? 0.0f : 1.5707963f;
        auto_yaw = base + (long_x ? -0.7f : 0.7f);

        // lighting terms (port of the engine bake)
        float el = env.sun_el * 3.14159265f / 180.f;
        float az = env.sun_az * 3.14159265f / 180.f;
        V3 sd = vnorm({cosf(az) * cosf(el), sinf(el), sinf(az) * cosf(el)});
        float sun[3] = {1.05f * env.intensity, 0.97f * env.intensity,
                        0.86f * env.intensity};
        float sky[3] = {0.35f + env.sky[0] * .5f, 0.42f + env.sky[1] * .5f,
                        0.52f + env.sky[2] * .5f};
        float horizon[3] = {fminf(1, sky[0] * .8f + .15f),
                            fminf(1, sky[1] * .8f + .14f),
                            fminf(1, sky[2] * .8f + .13f)};
        for (const Raw &r : raws) {
            float ndl = fabsf(vdot(r.n, sd));
            float amb = 0.34f + 0.22f * r.n.y;
            float c[3];
            for (int k = 0; k < 3; k++)
                c[k] = r.alb[k] * (sky[k] * amb + sun[k] * 0.85f * ndl);
            float dcx = (r.p[0].x + r.p[1].x + r.p[2].x) / 3 - centre[0];
            float dcy = (r.p[0].y + r.p[1].y + r.p[2].y) / 3 - centre[1];
            float dcz = (r.p[0].z + r.p[1].z + r.p[2].z) / 3 - centre[2];
            float d = sqrtf(dcx * dcx + dcy * dcy + dcz * dcz);
            float hf = (d / radius);
            hf = hf < 0 ? 0 : hf > 1 ? 1 : hf;
            float haze = hf * hf * 0.45f;
            Tri t;
            t.p[0] = r.p[0]; t.p[1] = r.p[1]; t.p[2] = r.p[2];
            for (int k = 0; k < 3; k++)
                t.col[k] = fminf(1, c[k] + (horizon[k] - c[k]) * haze);
            tris.push_back(t);
        }
        return true;
    }

    static const char *model_of(const LvJson *tags) {
        if (!tags || tags->kind != LJ_ARR) return nullptr;
        for (int i = 0; i < tags->count; i++) {
            const char *s = lvj_str(lvj_at(tags, i), NULL);
            if (s && !strncmp(s, "model:", 6)) return s + 6;
        }
        return nullptr;
    }
    static bool has_tag(const LvJson *tags, const char *t) {
        if (!tags || tags->kind != LJ_ARR) return false;
        for (int i = 0; i < tags->count; i++) {
            const char *s = lvj_str(lvj_at(tags, i), NULL);
            if (s && !strcmp(s, t)) return true;
        }
        return false;
    }
};

// ------------------------------------------------------------- rasterizer
static void render(const Scene &sc, FB &fb, float angle, float hmul) {
    const bool dbg = getenv("LITT_DEBUG") != NULL;
    float dbg_hx_min = 1e9f, dbg_hx_max = -1e9f;
    float dbg_hy_min = 1e9f, dbg_hy_max = -1e9f;
    long dbg_px = 0, rej_behind = 0, rej_area = 0,
         rej_bbox = 0, drawn_tris = 0;
    float r = sc.fit_r;
    // constant SLANT-range framing: elevation trades height vs ground dist
    float slant = r * 1.9f > 12 ? r * 1.9f : 12;
    float ext_y = sc.bounds_max[1] - sc.bounds_min[1];
    float ext_x = sc.bounds_max[0] - sc.bounds_min[0];
    float ext_z = sc.bounds_max[2] - sc.bounds_min[2];
    float ext_h = ext_x > ext_z ? ext_x : ext_z;
    bool flat_world = ext_y < 0.35f * ext_h;
    float el = (flat_world ? 55.0f : 24.0f) * 3.14159265f / 180.0f;
    float hgt = slant * sinf(el) * hmul;                       // camera lift
    float dist = sqrtf(slant * slant - hgt * hgt);             // ground dist
    V3 eye = {sc.centre[0] + cosf(angle) * dist, sc.centre[1] + hgt,
              sc.centre[2] + sinf(angle) * dist};
    float P[16], V[16], MVP[16];
    // 60-degree HORIZONTAL fov => derive vertical from aspect (studio.rs's
    // raw 1/tan convention implies a ~143 deg hfov - fine in a side panel,
    // fisheye in a game window)
    float aspect = (float)fb.w / fb.h;
    float vfov = 2.0f * atanf(tanf(30.0f * 3.14159265f / 180.0f) / aspect) *
                 180.0f / 3.14159265f;
    persp(vfov, aspect, 0.1f, 6000.f, P);
    look_at(eye, {sc.centre[0], sc.centre[1], sc.centre[2]}, V);
    mat_mul(P, V, MVP);

    auto xf = [&](V3 p, float *sx, float *sy, float *sz) {
        float cx = MVP[0] * p.x + MVP[4] * p.y + MVP[8] * p.z + MVP[12];
        float cy = MVP[1] * p.x + MVP[5] * p.y + MVP[9] * p.z + MVP[13];
        float cw = MVP[3] * p.x + MVP[7] * p.y + MVP[11] * p.z + MVP[15];
        float cz = MVP[2] * p.x + MVP[6] * p.y + MVP[10] * p.z + MVP[14];
        *sx = cx; *sy = cy; *sz = cz; // w kept separate below
        return cw;
    };
    for (const Tri &t : sc.tris) {
        float sx[3], sy[3], sz[3], sw[3];
        bool behind = false;
        for (int k = 0; k < 3; k++) {
            sw[k] = xf(t.p[k], &sx[k], &sy[k], &sz[k]);
            if (sw[k] < 0.05f) behind = true;
        }
        if (behind) {
            if (dbg) rej_behind++;
            continue;
        }
        float hx[3], hy[3];
        for (int k = 0; k < 3; k++) {
            float inv = 1.0f / sw[k];
            hx[k] = (sx[k] * inv * 0.5f + 0.5f) * fb.w;
            hy[k] = (0.5f - sy[k] * inv * 0.5f) * fb.h;
            sz[k] *= inv;
        }
        float area = (hx[1] - hx[0]) * (hy[2] - hy[0]) -
                     (hx[2] - hx[0]) * (hy[1] - hy[0]);
        if (dbg) {
            drawn_tris++;
            for (int k = 0; k < 3; k++) {
                if (hx[k] < dbg_hx_min) dbg_hx_min = hx[k];
                if (hx[k] > dbg_hx_max) dbg_hx_max = hx[k];
                if (hy[k] < dbg_hy_min) dbg_hy_min = hy[k];
                if (hy[k] > dbg_hy_max) dbg_hy_max = hy[k];
            }
        }
        if (fabsf(area) < 1e-6f) {
            if (dbg) rej_area++;
            continue;
        }
        int minx = (int)fmaxf(0, floorf(fminf(fminf(hx[0], hx[1]), hx[2])));
        int maxx = (int)fminf(fb.w - 1, ceilf(fmaxf(fmaxf(hx[0], hx[1]), hx[2])));
        int miny = (int)fmaxf(0, floorf(fminf(fminf(hy[0], hy[1]), hy[2])));
        int maxy = (int)fminf(fb.h - 1, ceilf(fmaxf(fmaxf(hy[0], hy[1]), hy[2])));
        if (minx > maxx || miny > maxy) {
            if (dbg) rej_bbox++;
            continue;
        }
        unsigned rr = (unsigned)(powf(t.col[0], 1.0f / 2.2f) * 255);
        unsigned gg = (unsigned)(powf(t.col[1], 1.0f / 2.2f) * 255);
        unsigned bb = (unsigned)(powf(t.col[2], 1.0f / 2.2f) * 255);
        unsigned color = 0xFF000000u | (rr << 16) | (gg << 8) | bb;

        // incremental barycentric: W0/W1 evaluated once per row start, then
        // stepped by constant deltas across the scanline. Deltas are the
        // raw-edge derivatives scaled by inv_area (they normalize WITH the
        // initial value).
        const float inv_area = 1.0f / area;
        const float st0x = (hy[1] - hy[2]) * inv_area;
        const float st0y = (hx[2] - hx[1]) * inv_area;
        const float st1x = (hy[2] - hy[0]) * inv_area;
        const float st1y = (hx[0] - hx[2]) * inv_area;
        const float fsx = (float)minx + 0.5f, fsy = (float)miny + 0.5f;
        float r0 = ((hx[1] - fsx) * (hy[2] - fsy) - (hx[2] - fsx) * (hy[1] - fsy)) * inv_area;
        float r1 = ((hx[2] - fsx) * (hy[0] - fsy) - (hx[0] - fsx) * (hy[2] - fsy)) * inv_area;
        for (int y = miny; y <= maxy; y++) {
            float w0 = r0, w1 = r1;
            unsigned *rowp = &fb.px[(size_t)y * fb.w];
            float *rowz = &fb.depth[(size_t)y * fb.w];
            for (int x = minx; x <= maxx; x++) {
                float w2 = 1.0f - w0 - w1;
                if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                    float z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
                    if (z < rowz[x]) {
                        rowz[x] = z;
                        rowp[x] = color;
                        if (dbg) dbg_px++;
                    }
                }
                w0 += st0x;
                w1 += st1x;
            }
            r0 += st0y;
            r1 += st1y;
        }
    }
    if (dbg)
        fprintf(stderr,
                "[dbg] tris=%zu behind_rej=%ld area_rej=%ld offscreen=%ld "
                "drawn=%ld written=%ld bbox_px hx=[%.0f..%.0f] hy=[%.0f..%.0f]"
                " fit_r=%.0f slant=%.0f\n",
                sc.tris.size(), rej_behind, rej_area, rej_bbox, drawn_tris,
                dbg_px, dbg_hx_min, dbg_hx_max, dbg_hy_min, dbg_hy_max,
                sc.fit_r, r * 1.9f);
}

// ------------------------------------------------------------------ win32
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static int run_window(const char *dir) {
    Scene sc;
    if (!sc.load(dir)) {
        fprintf(stderr, "[littview] failed to load %s\n", dir);
        return 1;
    }
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "LittView";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, "LittView", "Litt - C++ front-end",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                60, 60, 1280, 760, NULL, NULL, wc.hInstance, NULL);
    HDC wdc = GetDC(hwnd);
    HDC mdc = CreateCompatibleDC(wdc);
    FB fb;
    fb.resize(1200, 680);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = fb.w;
    bi.bmiHeader.biHeight = -fb.h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HBITMAP bmp = CreateDIBSection(mdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    SelectObject(mdc, bmp);

    float angle = 0.7f;
    // live-reload: re-read the world when world_state.json changes on disk
    std::filesystem::path ws = std::filesystem::path(dir) / "world_state.json";
    auto ws_time = [&]() -> std::filesystem::file_time_type {
        std::error_code ec;
        auto t = std::filesystem::last_write_time(ws, ec);
        return ec ? std::filesystem::file_time_type() : t;
    };
    auto last_mod = ws_time();
    MSG msg = {};
    for (;;) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT ||
                (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE))
                goto done;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (ws_time() != last_mod) {   // AI rewrote the world: reload it
            last_mod = ws_time();
            Scene fresh;
            if (fresh.load(dir)) sc = fresh;
        }
        angle += 0.0016f;
        fb.clear();
        render(sc, fb, angle, 1.0f);
        memcpy(bits, fb.px.data(), (size_t)fb.w * fb.h * 4);
        BitBlt(wdc, 0, 0, fb.w, fb.h, mdc, 0, 0, SRCCOPY);
        Sleep(16);
    }
done:
    DeleteObject(bmp);
    DeleteDC(mdc);
    ReleaseDC(hwnd, wdc);
    return 0;
}
#else
static int run_window(const char *dir) {
    (void)dir;
    fprintf(stderr, "window mode not supported on this platform yet\n");
    return 2;
}
#endif

// ------------------------------------------------------------------ main
#ifdef _WIN32
#include <windows.h>
static double now_s(void) {
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)f.QuadPart;
}
#else
#include <ctime>
static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}
#endif

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "selftest")) {
        // fallthrough to mode handling below
    } else if (argc < 3) {
        fprintf(stderr,
                "usage:\n"
                "  littview render <dir> [--yaw d] [--hgt f] [--w W] [--h H] [--out f.bmp]\n"
                "  littview bench  <dir> [--frames N]\n"
                "  littview window <dir>\n"
                "  littview selftest\n");
        return 2;
    }
    std::string mode = argv[1];

    if (mode == "selftest") {
        FB fb;
        fb.resize(64, 64);
        Scene fake;
        Tri t;
        // diamond facing the orbit camera (which starts on the +X axis)
        t.p[0] = {0, 1.2f, 0};
        t.p[1] = {0, -1.2f, 0.8f};
        t.p[2] = {0, -1.2f, -0.8f};
        t.col[0] = 1; t.col[1] = .4f; t.col[2] = .1f;
        fake.tris.push_back(t);
        fake.centre[0] = fake.centre[1] = fake.centre[2] = 0;
        fake.radius = 2;
        render(fake, fb, 0.0f, 1.0f);
        unsigned c = fb.px[(size_t)32 * 64 + 32];
        int hits = 0;
        int minx = 999, maxx = -1, miny = 999, maxy = -1;
        for (int y = 0; y < fb.h; y++)
            for (int x = 0; x < fb.w; x++) {
                unsigned p = fb.px[(size_t)y * fb.w + x];
                if ((p & 0x00FFFFFF) != (0x20180F)) {
                    hits++;
                    if (x < minx) minx = x;
                    if (x > maxx) maxx = x;
                    if (y < miny) miny = y;
                    if (y > maxy) maxy = y;
                }
            }
        bool hit = (c >> 16) > 100 && ((c >> 8) & 255) > 30;
        printf("selftest center=%06X hits=%d bbox=[%d..%d]x[%d..%d] %s\n", c,
               hits, minx, maxx, miny, maxy, hit ? "ok" : "FAIL");
        return hit ? 0 : 1;
    }

    const char *dir = argv[2];
    float yaw = -1000.0f, hgt = 1.0f; // yaw<0 => auto (perpendicular to long axis)
    int W = 960, H = 540;
    const char *out = "frame.bmp";
    for (int i = 3; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--yaw")) yaw = (float)atof(argv[i + 1]);
        else if (!strcmp(argv[i], "--hgt")) hgt = (float)atof(argv[i + 1]);
        else if (!strcmp(argv[i], "--w")) W = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--h")) H = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--out")) out = argv[i + 1];
    }

    if (mode == "window") return run_window(dir);
    if (mode != "render" && mode != "bench") {
        fprintf(stderr, "unknown mode %s\n", argv[1]);
        return 2;
    }

    Scene sc;
    if (!sc.load(dir)) {
        fprintf(stderr, "[littview] failed to load %s\n", dir);
        return 1;
    }
    if (yaw < -999.0f) yaw = sc.auto_yaw;
    FB fb;
    fb.resize(W, H);

    if (mode == "bench") {
        int frames = 200;
        for (int i = 3; i + 1 < argc; i += 2)
            if (!strcmp(argv[i], "--frames")) frames = atoi(argv[i + 1]);
        double t0 = now_s();
        for (int i = 0; i < frames; i++) {
            fb.clear();
            render(sc, fb, yaw + i * 0.02f, hgt);
        }
        double dt = now_s() - t0;
        printf("{\"bench\":true,\"frames\":%d,\"ms_per_frame\":%.3f,"
               "\"tris\":%zu,\"w\":%d,\"h\":%d}\n",
               frames, dt * 1000.0 / frames, sc.tris.size(), W, H);
        return 0;
    }

    render(sc, fb, yaw, hgt);
    if (!write_bmp(out, fb)) {
        fprintf(stderr, "[littview] cannot write %s\n", out);
        return 1;
    }
    printf("{\"ok\":true,\"tris\":%zu,\"w\":%d,\"h\":%d,\"missing\":%ld,\"out\":\"%s\"}\n",
           sc.tris.size(), W, H, sc.missing, out);
    return 0;
}
