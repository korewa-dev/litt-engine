/* litt_obj.c */
#include "litt_obj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float *v; int n, cap;
} FVec;

static void fv_push(FVec *a, float x) {
    if (a->n == a->cap) {
        a->cap = a->cap ? a->cap * 2 : 64;
        a->v = realloc(a->v, sizeof(float) * (size_t)a->cap);
    }
    a->v[a->n++] = x;
}

typedef struct {
    unsigned *v; int n, cap;
} IVec;

static void iv_push(IVec *a, unsigned x) {
    if (a->n == a->cap) {
        a->cap = a->cap ? a->cap * 2 : 64;
        a->v = realloc(a->v, sizeof(unsigned) * (size_t)a->cap);
    }
    a->v[a->n++] = x;
}

/* global (pos,norm) pair -> local vertex slot within current mesh */
typedef struct { int gpos, gnorm; unsigned local; } Remap;
typedef struct { Remap *v; int n, cap; } RMap;

static int rmap_get(RMap *m, int gpos, int gnorm, unsigned *out) {
    for (int i = 0; i < m->n; i++)
        if (m->v[i].gpos == gpos && m->v[i].gnorm == gnorm) { *out = m->v[i].local; return 1; }
    return 0;
}

static void rmap_put(RMap *m, int gpos, int gnorm, unsigned local) {
    if (m->n == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 64;
        m->v = realloc(m->v, sizeof(Remap) * (size_t)m->cap);
    }
    m->v[m->n++] = (Remap){ gpos, gnorm, local };
}

void lv_model_free(LvModel *m) {
    if (!m) return;
    for (int i = 0; i < m->count; i++) {
        free(m->meshes[i].verts);
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
    FVec gp = {0}, gn = {0};
    FVec cv = {0};           /* current mesh verts (xyz) */
    IVec ci = {0};           /* current mesh indices */
    RMap rm = {0};
    char cur_name[64] = "obj_mesh";
    int any_face = 0;

#define FLUSH()                                                          \
    do {                                                                 \
        if (cv.n > 0) {                                                  \
            model.meshes = realloc(model.meshes,                         \
                sizeof(LvMesh) * (size_t)(model.count + 1));             \
            LvMesh *me = &model.meshes[model.count];                     \
            memset(me, 0, sizeof(*me));                                  \
            snprintf(me->name, sizeof(me->name), "%s", cur_name);        \
            me->verts = cv.v; me->vn = cv.n / 3;                         \
            me->idx = ci.v; me->in = ci.n;                               \
            mesh_bounds(me);                                             \
            model.count++;                                               \
            cv.v = NULL; cv.n = cv.cap = 0;                              \
            ci.v = NULL; ci.n = ci.cap = 0;                              \
            rm.n = 0;                                                    \
            any_face = 1;                                                \
        }                                                                \
    } while (0)

    char *line = buf;
    char *end = buf + rd;
    while (line < end) {
        char *nl = memchr(line, '\n', (size_t)(end - line));
        char *next = nl ? nl + 1 : end;
        if (nl) *nl = 0;   /* last line may lack '\n' - never deref NULL */
        /* trim */
        while (*line == ' ' || *line == '\t' || *line == '\r') line++;
        if (*line == 0 || *line == '#') { line = next; continue; }

        if (!strncmp(line, "v ", 2)) {
            float x, y, z;
            if (sscanf(line + 2, "%f %f %f", &x, &y, &z) == 3) {
                fv_push(&gp, x); fv_push(&gp, y); fv_push(&gp, z);
            }
        } else if (!strncmp(line, "vn ", 3)) {
            float x, y, z;
            if (sscanf(line + 3, "%f %f %f", &x, &y, &z) == 3) {
                fv_push(&gn, x); fv_push(&gn, y); fv_push(&gn, z);
            }
        } else if (!strncmp(line, "g ", 2) || !strncmp(line, "o ", 2)) {
            FLUSH();
            const char *nm = line + 2;
            while (*nm == ' ') nm++;
            snprintf(cur_name, sizeof(cur_name), "%s", nm);
            char *sp = strchr(cur_name, ' ');
            if (sp) *sp = 0;
        } else if (!strncmp(line, "usemtl ", 7)) {
            /* material switch splits only when no explicit groups yet */
            FLUSH();
        } else if (!strncmp(line, "f ", 2)) {
            unsigned corners[64];
            int nc = 0;
            char *tok = line + 2;
            while (*tok && nc < 64) {
                while (*tok == ' ') tok++;
                if (!*tok) break;
                int vi = 0;
                int got = sscanf(tok, "%d", &vi);
                if (!got || vi < 1) break;
                /* skip vt/vn slots */
                while (*tok && *tok != ' ') tok++;
                corners[nc++] = (unsigned)(vi - 1);
            }
            if (nc >= 3) {
                for (int i = 2; i < nc; i++) {
                    unsigned tri[3] = { corners[0], corners[i - 1], corners[i] };
                    for (int t = 0; t < 3; t++) {
                        unsigned local;
                        if (!rmap_get(&rm, (int)tri[t], -1, &local)) {
                            local = (unsigned)(cv.n / 3);
                            rmap_put(&rm, (int)tri[t], -1, local);
                            for (int k = 0; k < 3; k++) {
                                int gi = (int)tri[t] * 3 + k;
                                fv_push(&cv, gi < gp.n ? gp.v[gi] : 0.0f);
                            }
                        }
                        iv_push(&ci, local);
                    }
                }
            }
        }
        line = next;
    }
    FLUSH();
#undef FLUSH

    free(rm.v);
    free(gp.v);
    free(gn.v);
    free(cv.v);
    free(ci.v);
    free(buf);

    if (model.count == 0 || !any_face) {
        lv_model_free(&model);
        return 1;
    }
    *out = model;
    return 0;
}
