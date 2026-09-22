/* litt_obj.h - group-aware OBJ loader.
 * Every `g <name>` / `usemtl` switch starts a named mesh; faces may span
 * global vertex indices and are remapped per mesh. */
#ifndef LITT_OBJ_H
#define LITT_OBJ_H

#ifdef __cplusplus
extern "C" {
#endif

#define LV_OBJ_MAX_FILE_BYTES (32u * 1024u * 1024u)
#define LV_MTL_MAX_FILE_BYTES (4u * 1024u * 1024u)
#define LV_OBJ_MAX_FLOAT_VALUES 9000000
#define LV_OBJ_MAX_INDICES 3000000
#define LV_OBJ_MAX_REMAP 3000000
#define LV_OBJ_MAX_MESHES 65536

typedef struct {
    char name[64];
    float *verts;      /* xyz triplets */
    int vn;
    unsigned *idx;
    int in;            /* index count */
    float bmin[3], bmax[3];
    /* ASSET_AUDIT 4.1/4.2: per-usemtl-group material resolved from the
     * mtllib-linked MTL file. Flat Kd albedo + Ke emission only; has_* is
     * 0 when no MTL/material applies so callers keep their own fallback
     * tinting. uv table: u,v pairs parallel to verts (uv[i*2..i*2+1] for
     * vertex i); NULL when the mesh has no texture coordinates. */
    float kd[3], ke[3];
    char map_kd[160]; /* diffuse texture path from MTL, relative to MTL/OBJ dir */
    float *uvs;        /* uv triplets->pairs, vn entries or NULL */
    unsigned char has_kd, has_ke, has_map_kd;
} LvMesh;

typedef struct {
    LvMesh *meshes;
    int count;
} LvModel;

/* 0 = ok, nonzero = error (file unreadable / no faces). */
int lv_obj_load(const char *path, LvModel *out);
void lv_model_free(LvModel *m);

#ifdef __cplusplus
}
#endif

#endif
