/* litt_obj.c */
#include "litt_obj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    float *v; int n, cap;
} FVec;

static int fv_push(FVec *a, float x) {
    if (!a || !isfinite(x) || a->n < 0 || a->n >= LV_OBJ_MAX_FLOAT_VALUES) return 1;
    if (a->n == a->cap) {
        int nc = a->cap ? a->cap : 64;
        if (nc > LV_OBJ_MAX_FLOAT_VALUES / 2) nc = LV_OBJ_MAX_FLOAT_VALUES;
        else nc *= 2;
        if (nc <= a->cap || nc > LV_OBJ_MAX_FLOAT_VALUES) return 1;
        float *nv = realloc(a->v, sizeof(float) * (size_t)nc);
        if (!nv) return 1;
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
    if (!a || a->n < 0 || a->n >= LV_OBJ_MAX_INDICES) return 1;
    if (a->n == a->cap) {
        int nc = a->cap ? a->cap : 64;
        if (nc > LV_OBJ_MAX_INDICES / 2) nc = LV_OBJ_MAX_INDICES;
        else nc *= 2;
        if (nc <= a->cap || nc > LV_OBJ_MAX_INDICES) return 1;
        unsigned *nv = realloc(a->v, sizeof(unsigned) * (size_t)nc);
        if (!nv) return 1;
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
    if (!m || m->n < 0 || m->n >= LV_OBJ_MAX_REMAP) return 1;
    if (m->n == m->cap) {
        int nc = m->cap ? m->cap : 64;
        if (nc > LV_OBJ_MAX_REMAP / 2) nc = LV_OBJ_MAX_REMAP;
        else nc *= 2;
        if (nc <= m->cap || nc > LV_OBJ_MAX_REMAP) return 1;
        Remap *nv = realloc(m->v, sizeof(Remap) * (size_t)nc);
        if (!nv) return 1;
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
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return; }
    long mtl_size = ftell(f);
    if (mtl_size <= 0 || (unsigned long)mtl_size > (unsigned long)LV_MTL_MAX_FILE_BYTES) {
        fclose(f);
        return;
    }
    rewind(f);
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
            if (*cur && sscanf(p + 3, "%f %f %f", &r, &g, &b) == 3 &&
                isfinite(r) && isfinite(g) && isfinite(b)) {
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
    if (!path || !*path || !out) return 1;
    out->meshes = NULL;
    out->count = 0;

    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 1; }
    long sz = ftell(f);
    if (sz <= 0 || (unsigned long)sz > (unsigned long)LV_OBJ_MAX_FILE_BYTES) {
        fclose(f);
        return 1;
    }
    rewind(f);
    char *buf = malloc((size_t)sz + 1u);
    if (!buf) { fclose(f); return 1; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    if (rd != (size_t)sz || ferror(f)) {
        free(buf);
        fclose(f);
        return 1;
    }
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
    int oom = 0;
    int malformed = 0;                         /* m4: any allocation failure */
    LvMtlLib lib;                        /* MTL side-table (4.1/4.2) */
    char cur_mtl[64] = "";
    memset(&lib, 0, sizeof(lib));

#define FLUSH()                                                          \
    do {                                                                 \
        if (cv.n > 0) {                                                  \
            if (model.count >= LV_OBJ_MAX_MESHES) { oom = 1; break; }    \
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
                me->uvs = ct.n == me->vn * 2 ? ct.v : NULL;                    \
                if (me->uvs) { ct.v = NULL; ct.n = ct.cap = 0; }         \
                mesh_bounds(me);                                         \
                {   LvMtl *mt = mtl_find(&lib, cur_mtl);                 \
                    if (mt) {                                            \
                        me->kd[0] = mt->kd[0]; me->kd[1] = mt->kd[1];    \
                        me->kd[2] = mt->kd[2];                           \
                        me->ke[0] = mt->ke[0]; me->ke[1] = mt->ke[1];    \
                        me->ke[2] = mt->ke[2];                           \
                        me->has_kd = mt->has_kd; me->has_ke = mt->has_ke;\
                        me->has_map_kd = mt->has_map;                      \
                        if (mt->has_map) snprintf(me->map_kd,             \
                            sizeof(me->map_kd), "%s", mt->map_kd);       \
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
    while (line < end && !oom && !malformed) {
        char *nl = memchr(line, '\n', (size_t)(end - line));
        char *next = nl ? nl + 1 : end;
        if (nl) *nl = 0;   /* last line may lack '\n' - never deref NULL */
        /* trim */
        while (*line == ' ' || *line == '\t' || *line == '\r') line++;
        if (*line == 0 || *line == '#') { line = next; continue; }

        if (!strncmp(line, "v ", 2)) {
            float x, y, z;
            if (sscanf(line + 2, "%f %f %f", &x, &y, &z) != 3 ||
                !isfinite(x) || !isfinite(y) || !isfinite(z)) {
                malformed = 1;
            } else {
                if (fv_push(&gp, x) || fv_push(&gp, y) || fv_push(&gp, z)) oom = 1;
            }
        } else if (!strncmp(line, "vt ", 3)) {
            float u, v;
            if (sscanf(line + 3, "%f %f", &u, &v) != 2 ||
                !isfinite(u) || !isfinite(v)) {
                malformed = 1;
            } else if (fv_push(&gt, u) || fv_push(&gt, v)) {
                oom = 1;
            }
        } else if (!strncmp(line, "vn ", 3)) {
            float x, y, z;
            if (sscanf(line + 3, "%f %f %f", &x, &y, &z) != 3 ||
                !isfinite(x) || !isfinite(y) || !isfinite(z)) {
                malformed = 1;
            } else if (fv_push(&gn, x) || fv_push(&gn, y) || fv_push(&gn, z)) {
                oom = 1;
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
            int uvtex[64];
            int normals[64];
            int nc = 0;
            char *tok = line + 2;
            while (*tok) {
                while (*tok == ' ' || *tok == '\t') tok++;
                if (!*tok) break;
                if (nc >= 64) { malformed = 1; break; }

                char *token_end = tok;
                while (*token_end && *token_end != ' ' && *token_end != '\t') token_end++;
                size_t token_len = (size_t)(token_end - tok);
                if (token_len == 0 || token_len >= 128u) { malformed = 1; break; }

                char spec[128];
                memcpy(spec, tok, token_len);
                spec[token_len] = 0;

                int vi = 0, tv = 0, tn = 0, consumed = 0;
                int has_uv = 0, has_normal = 0;
                if (sscanf(spec, "%d/%d/%d%n", &vi, &tv, &tn, &consumed) == 3 &&
                    spec[consumed] == 0) {
                    has_uv = 1; has_normal = 1;
                } else if (sscanf(spec, "%d//%d%n", &vi, &tn, &consumed) == 2 &&
                           spec[consumed] == 0) {
                    has_normal = 1;
                } else if (sscanf(spec, "%d/%d%n", &vi, &tv, &consumed) == 2 &&
                           spec[consumed] == 0) {
                    has_uv = 1;
                } else if (!(sscanf(spec, "%d%n", &vi, &consumed) == 1 &&
                             spec[consumed] == 0)) {
                    malformed = 1; break;
                }

                const int pos_count = gp.n / 3;
                if (vi < 0) vi += pos_count + 1;
                if (vi < 1 || vi > pos_count) { malformed = 1; break; }

                uvtex[nc] = -1;
                if (has_uv) {
                    const int uv_count = gt.n / 2;
                    if (tv < 0) tv += uv_count + 1;
                    if (tv < 1 || tv > uv_count) { malformed = 1; break; }
                    uvtex[nc] = tv - 1;
                }

                normals[nc] = -1;
                if (has_normal) {
                    const int normal_count = gn.n / 3;
                    if (tn < 0) tn += normal_count + 1;
                    if (tn < 1 || tn > normal_count) { malformed = 1; break; }
                    normals[nc] = tn - 1;
                }

                corners[nc++] = (unsigned)(vi - 1);
                tok = token_end;
            }

            if (!malformed && nc < 3) malformed = 1;
            if (!malformed) {
                for (int i = 2; i < nc && !oom; i++) {
                    unsigned tri[3] = { corners[0], corners[i - 1], corners[i] };
                    int tri_t[3] = { uvtex[0], uvtex[i - 1], uvtex[i] };
                    int tri_n[3] = { normals[0], normals[i - 1], normals[i] };
                    for (int t = 0; t < 3; t++) {
                        unsigned local;
                        if (!rmap_get(&rm, (int)tri[t], tri_t[t], tri_n[t], &local)) {
                            local = (unsigned)(cv.n / 3);
                            if (rmap_put(&rm, (int)tri[t], tri_t[t], tri_n[t], local)) {
                                oom = 1; break;
                            }
                            const size_t pos = (size_t)tri[t] * 3u;
                            if (pos + 2u >= (size_t)gp.n ||
                                fv_push(&cv, gp.v[pos]) ||
                                fv_push(&cv, gp.v[pos + 1u]) ||
                                fv_push(&cv, gp.v[pos + 2u])) {
                                oom = 1; break;
                            }
                            if (tri_t[t] >= 0) {
                                const size_t uv = (size_t)tri_t[t] * 2u;
                                if (uv + 1u >= (size_t)gt.n ||
                                    fv_push(&ct, gt.v[uv]) ||
                                    fv_push(&ct, gt.v[uv + 1u])) {
                                    oom = 1; break;
                                }
                            } else if (fv_push(&ct, 0.0f) || fv_push(&ct, 0.0f)) {
                                oom = 1; break;
                            }
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
    if (oom || malformed || model.count == 0 || !any_face) {
        lv_model_free(&model);
        return 1;
    }
    *out = model;
    return 0;
}
