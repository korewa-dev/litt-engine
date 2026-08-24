/* litt_obj.h - group-aware OBJ loader (port of crates/asset model.rs).
 * Every `g <name>` / `usemtl` switch starts a named mesh; faces may span
 * global vertex indices and are remapped per mesh. */
#ifndef LITT_OBJ_H
#define LITT_OBJ_H

typedef struct {
    char name[64];
    float *verts;      /* xyz triplets */
    int vn;
    unsigned *idx;
    int in;            /* index count */
    float bmin[3], bmax[3];
} LvMesh;

typedef struct {
    LvMesh *meshes;
    int count;
} LvModel;

/* 0 = ok, nonzero = error (file unreadable / no faces). */
int lv_obj_load(const char *path, LvModel *out);
void lv_model_free(LvModel *m);

#endif
