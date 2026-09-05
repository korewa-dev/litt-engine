/* litt_obj.c */
#include "litt_obj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float *v; int n, cap;
} FVec;

static int fv_push(FVec *a, float x) {
    if (a->n == a->cap) {
        int nc = a->cap ? a->cap * 2 : 64;
        float *nv = realloc(a->v, sizeof(float) * (size_t)nc);
        if (!nv) return 1;               /* m4: old buffer intact, caller errs */
        a->v = nv;
        a->cap = nc;
    }
    a->v[a->n++] = x;
    return 0;
}

typedef struct {
    unsigned *v; int n, cap;
} IVec;

static int iv_push(IVec *a, unsigned x) {
    if (a->n == a->cap) {
        int nc = a->cap ? a->cap * 2 : 64;
        unsigned *nv = realloc(a->v, sizeof(unsigned) * (size_t)nc);
        if (!nv) return 1;               /* m4 */
        a->v = nv;
        a->cap = nc;
    }
    a->v[a->n++] = x;
    return 0;
}

/* global (pos,uv,norm) triple -> local vertex slot within current mesh */
typedef struct { int gpos, guv, gnorm; unsigned local; } Remap;
typedef struct { Remap *v; int n, cap; } RMap;

static int rmap_get(RMap *m, int gpos, int guv, int gnorm, unsigned *out) {
    for (int i = 0; i < m->n; i++)
        if (m->v[i].gpos == gpos && m->v[i].guv == guv &&
            m->v[i].gnorm == gnorm) { *out = m->v[i].local; return 1; }
    return 0;
}

static int rmap_put(RMap *m, int gpos, int guv, int gnorm, unsigned local) {
    if (m->n == m->cap) {
        int nc = m->cap ? m->cap * 2 : 64;
        Remap *nv = realloc(m->v, sizeof(Remap) * (size_t)nc);
        if (!nv) return 1;               /* m4 */
        m->v = nv;
        m->cap = nc;
    }
    m->v[m->n++] = (Remap){ gpos, guv, gnorm, local };
    return 0;
}

/* ---- per-material color side-table (ASSET_AUDIT 4.1/4.2) --------------
 * Parses mtllib-linked MTL files for flat Kd (albedo), Ke (emission) and
 * map_Kd (diffuse texture path, kept for the renderer; no image decoding
 * here). Ka/Ks/Ns/d/illum are deliberately ignored. */
typedef struct {
    char name[64];
    float kd[3], ke[3];
    char map_kd[160];
    unsigned char has_kd, has_ke, has_map;
} LvMtl;

#define LV_MTL_MAX 64
typedef struct {
    LvMtl m[LV_MTL_MAX];
    int n;
} LvMtlLib;

static LvMtl *mtl_find(LvMtlLib *lib, const char *name) {
    if (!name || !name[0]) return NULL;
    for (int i = 0; i < lib->n; i++)
        if (!strncmp(lib->m[i].name, name, sizeof(lib->m[i].name)))
            return &lib->m[i];
    return NULL;
}

/* Resolve `mtllib` relative to the OBJ's own directory and merge its
 * materials into `lib`. Unreadable files are silently ignored so the
 * loader keeps today's geometry-only behavior as the fallback. */
static void lv_mtl_load(const char *obj_path, const char *mtllib,
                        LvMtlLib *lib) {
    char path[1024];
    const char *slash = strrchr(obj_path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(obj_path, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
    if (slash) {
        int dl = (int)(slash - obj_path);
        if (dl <= 0 || dl > 900) return;
        snprintf(path, sizeof(path), "%.*s/%s", dl, obj_path, mtllib);
    } else {
        snprintf(path, sizeof(path), "%s", mtllib);
    }
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char line[512];
    char cur[64] = "";
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!strncmp(p, "newmtl ", 7)) {
            const char *nm = p + 7;
            while (*nm == ' ' || *nm == '\t') nm++;
            snprintf(cur, sizeof(cur), "%s", nm);
            char *sp = strpbrk(cur, " \t\r\n");
            if (sp) *sp = 0;
            if (!mtl_find(lib, cur) && lib->n < LV_MTL_MAX) {
                LvMtl *e = &lib->m[lib->n++];
                memset(e, 0, sizeof(*e));
                snprintf(e->name, sizeof(e->name), "%s", cur);
            }
        } else if (!strncmp(p, "map_Kd ", 7)) {
            const char *nm = p + 7;
            while (*nm == ' ' || *nm == '\t') nm++;
            if (*cur && *nm) {
                LvMtl *e = mtl_find(lib, cur);
                if (e) {
                    snprintf(e->map_kd, sizeof(e->map_kd), "%s", nm);
                    char *sp2 = strpbrk(e->map_kd, " \t\r\n");
                    if (sp2) *sp2 = 0;
                    e->has_map = 1;
                }
            }
        } else if (!strncmp(p, "Kd ", 3) || !strncmp(p, "Ke ", 3)) {
            float r, g, b;
            if (*cur && sscanf(p + 3, "%f %f %f", &r, &g, &b) == 3) {
                LvMtl *e = mtl_find(lib, cur);
                if (e) {
                    float *dst = (p[1] == 'd') ? e->kd : e->ke;
                    dst[0] = r; dst[1] = g; dst[2] = b;
                    if (p[1] == 'd') e->has_kd = 1; else e->has_ke = 1;
                }
            }
        }
    }
    fclose(f);
}

void lv_model_free(LvModel *m) {
    if (!m) return;
    for (int i = 0; i < m->count; i++) {
        free(m->meshes[i].verts);
        free(m->meshes[i].uvs);
        free(m->meshes[i].idx);
    }
    free(m->meshes);
    m->meshes = NULL;
    m->count = 0;
}

static void mesh_bounds(LvMesh *me) {
    for (int k = 0; k < 3; k++) { me->bmin[k] = 1e9f; me->bmax[k] = -1e9f; }
    for (int i = 0; i + 2 < me->vn * 3; i += 3)
        for (int k = 0; k < 3; k++) {
            float x = me->verts[i + k];
            if (x < me->bmin[k]) me->bmin[k] = x;
            if (x > me->bmax[k]) me->bmax[k] = x;
        }
}

int lv_obj_load(const char *path, LvModel *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 1; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 1; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = 0;

    LvModel model = { NULL, 0 };
    /* growable global pools */
    FVec gp = {0}, gn = {0}, gt = {0};
    FVec cv = {0};           /* current mesh verts (xyz) */
    FVec ct = {0};           /* current mesh uvs (uv pairs, parallel to cv) */
    IVec ci = {0};           /* current mesh indices */
    RMap rm = {0};
    char cur_name[64] = "obj_mesh";
    int any_face = 0;
    int oom = 0;                         /* m4: any allocation failure */
    LvMtlLib lib;                        /* MTL side-table (4.1/4.2) */
    char cur_mtl[64] = "";
    memset(&lib, 0, sizeof(lib));

#define FLUSH()                                                          \
    do {                                                                 \
        if (cv.n > 0) {                                                  \
            LvMesh *nm = realloc(model.meshes,                           \
                sizeof(LvMesh) * (size_t)(model.count + 1));             \
            if (!nm) {                                                   \
                oom = 1;   /* m4: cv/ci stay locally owned, freed below */\
            } else {                                                     \
                LvMesh *me = nm + model.count;                           \
                model.meshes = nm;                                       \
                memset(me, 0, sizeof(*me));                              \
                snprintf(me->name, sizeof(me->name), "%s", cur_name);    \
                me->verts = cv.v; me->vn = cv.n / 3;                     \
                me->idx = ci.v; me->in = ci.n;                           \
                me->uvs = ct.n == cv.n ? ct.v : NULL;                    \
                if (me->uvs) { ct.v = NULL; ct.n = ct.cap = 0; }         \
                mesh_bounds(me);                                         \
                {   LvMtl *mt = mtl_find(&lib, cur_mtl);                 \
                    if (mt) {                                            \
                        me->kd[0] = mt->kd[0]; me->kd[1] = mt->kd[1];    \
                        me->kd[2] = mt->kd[2];                           \
                        me->ke[0] = mt->ke[0]; me->ke[1] = mt->ke[1];    \
                        me->ke[2] = mt->ke[2];                           \
                        me->has_kd = mt->has_kd; me->has_ke = mt->has_ke;\
                    }                                                    \
                }                                                        \
                model.count++;                                           \
                cv.v = NULL; cv.n = cv.cap = 0;                          \
                ci.v = NULL; ci.n = ci.cap = 0;                          \
                rm.n = 0;                                                \
                any_face = 1;                                            \
            }                                                            \
        }                                                                \
    } while (0)

    char *line = buf;
    char *end = buf + rd;
    while (line < end && !oom) {
        char *nl = memchr(line, '\n', (size_t)(end - line));
        char *next = nl ? nl + 1 : end;
        if (nl) *nl = 0;   /* last line may lack '\n' - never deref NULL */
        /* trim */
        while (*line == ' ' || *line == '\t' || *line == '\r') line++;
        if (*line == 0 || *line == '#') { line = next; continue; }

        if (!strncmp(line, "v ", 2)) {
            float x, y, z;
            if (sscanf(line + 2, "%f %f %f", &x, &y, &z) == 3) {
                if (fv_push(&gp, x)) oom = 1;
                if (fv_push(&gp, y)) oom = 1;
                if (fv_push(&gp, z)) oom = 1;
            }
        } else if (!strncmp(line, "vt ", 3)) {
            float u, v;
            if (sscanf(line + 3, "%f %f", &u, &v) == 2) {
                if (fv_push(&gt, u)) oom = 1;
                if (fv_push(&gt, v)) oom = 1;
            }
        } else if (!strncmp(line, "vn ", 3)) {
            float x, y, z;
            if (sscanf(line + 3, "%f %f %f", &x, &y, &z) == 3) {
                if (fv_push(&gn, x)) oom = 1;
                if (fv_push(&gn, y)) oom = 1;
                if (fv_push(&gn, z)) oom = 1;
            }
        } else if (!strncmp(line, "g ", 2) || !strncmp(line, "o ", 2)) {
            FLUSH();
            const char *nm = line + 2;
            while (*nm == ' ') nm++;
            snprintf(cur_name, sizeof(cur_name), "%s", nm);
            char *sp = strchr(cur_name, ' ');
            if (sp) *sp = 0;
        } else if (!strncmp(line, "mtllib ", 7)) {
            char lib_name[256];
            const char *nm = line + 7;
            while (*nm == ' ' || *nm == '\t') nm++;
            snprintf(lib_name, sizeof(lib_name), "%s", nm);
            char *sp2 = strpbrk(lib_name, " \t\r\n");
            if (sp2) *sp2 = 0;
            lv_mtl_load(path, lib_name, &lib);
        } else if (!strncmp(line, "usemtl ", 7)) {
            /* material switch splits only when no explicit groups yet */
            FLUSH();
            const char *mm = line + 7;
            while (*mm == ' ' || *mm == '\t') mm++;
            snprintf(cur_mtl, sizeof(cur_mtl), "%s", mm);
            char *sp3 = strpbrk(cur_mtl, " \t\r\n");
            if (sp3) *sp3 = 0;
        } else if (!strncmp(line, "f ", 2)) {
            unsigned corners[64];
            int uvtex[64];                       /* 0-based vt idx, -1 none */
            int nc = 0;
            char *tok = line + 2;
            while (*tok && nc < 64) {
                while (*tok == ' ' || *tok == '\t') tok++;  /* n4: tabs split too */
                if (!*tok) break;
                int vi = 0;
                int got = sscanf(tok, "%d", &vi);
                if (!got) break;
                if (vi < 0) vi += (int)(gp.n / 3) + 1;      /* n4: OBJ relative wrap */
                if (vi < 1) break;     /* 0 or out-of-range negative: drop face */
                /* optional /vt[/vn] slots after the position index */
                uvtex[nc] = -1;
                const char *slash = strchr(tok, '/');
                if (slash && slash[1] && slash[1] != ' ' && slash[1] != '\t' &&
                    slash[1] != '/') {
                    int tv;
                    if (sscanf(slash + 1, "%d", &tv) == 1) {
                        if (tv < 0) tv += (int)(gt.n / 2) + 1;  /* relative wrap */
                        if (tv >= 1) uvtex[nc] = tv - 1;
                    }
                }
                /* skip to next whitespace-delimited token */
                while (*tok && *tok != ' ' && *tok != '\t') tok++;
                corners[nc++] = (unsigned)(vi - 1);
            }
            if (nc >= 3) {
                for (int i = 2; i < nc && !oom; i++) {
                    unsigned tri[3] = { corners[0], corners[i - 1], corners[i] };
                    int tri_t[3] = { uvtex[0], uvtex[i - 1], uvtex[i] };
                    for (int t = 0; t < 3; t++) {
                        unsigned local;
                        if (!rmap_get(&rm, (int)tri[t],
                                      tri_t[t] < 0 ? -1 : tri_t[t], -1, &local)) {
                            local = (unsigned)(cv.n / 3);
                            if (rmap_put(&rm, (int)tri[t],
                                         tri_t[t] < 0 ? -1 : tri_t[t], -1,
                                         local)) { oom = 1; break; }
                            for (int k = 0; k < 3; k++) {
                                size_t gi = (size_t)tri[t] * 3u + (size_t)k; /* n4: no signed overflow */
                                if (fv_push(&cv, gi < (size_t)gp.n ? gp.v[gi] : 0.0f)) {
                                    oom = 1;
                                    break;
                                }
                            }
                            if (oom) break;
                            if (tri_t[t] >= 0) {
                                size_t gi = (size_t)tri_t[t] * 2u;
                                if (gi < (size_t)gt.n) {
                                    if (fv_push(&ct, gt.v[gi])) oom = 1;
                                    if (!oom && fv_push(&ct, gi + 1 < (size_t)gt.n
                                                        ? gt.v[gi + 1] : 0.0f)) oom = 1;
                                } else {
                                    if (fv_push(&ct, 0.0f)) oom = 1;
                                    if (!oom && fv_push(&ct, 0.0f)) oom = 1;
                                }
                            } else {
                                /* keep the uv table parallel even when this
                                 * face has no vt: emit (0,0) so index math
                                 * stays uniform for consumers */
                                if (fv_push(&ct, 0.0f)) oom = 1;
                                if (!oom && fv_push(&ct, 0.0f)) oom = 1;
                            }
                            if (oom) break;
                        }
                        if (iv_push(&ci, local)) { oom = 1; break; }
                    }
                }
            }
        }
        line = next;
    }
    FLUSH();
#undef FLUSH

    free(rm.v); rm.v = NULL;
    free(gp.v); gp.v = NULL;
    free(gn.v); gn.v = NULL;
    free(gt.v); gt.v = NULL;
    free(cv.v); cv.v = NULL;   /* NULL when FLUSH handed ownership off */
    free(ct.v); ct.v = NULL;   /* NULL when FLUSH handed ownership off */
    free(ci.v); ci.v = NULL;
    free(buf);

    /* m4: OOM or no usable geometry -> clean error, nothing leaked */
    if (oom || model.count == 0 || !any_face) {
        lv_model_free(&model);
        return 1;
    }
    *out = model;
    return 0;
}
