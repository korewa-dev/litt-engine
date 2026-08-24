/* littcli.c - headless world validator (C port of the play_native smoke).
 *
 *   littcli validate <dir> [--frames N]
 *
 * Loads <dir>/world_state.json + <dir>/assets/scenes/world.lscn.json,
 * builds solids/interactives, simulates N ticks at 60 Hz, prints the
 * compatibility line plus a machine-readable JSON tail. Exit 0 = valid. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "littcore/litt_world.h"

static int read_file(const char *path, char **out) {
    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 1; }
    char *buf = malloc((size_t)sz + 1);
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = 0;
    *out = buf;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3 || strcmp(argv[1], "validate")) {
        fprintf(stderr,
                "usage: littcli validate <game-dir> [--frames N]\n");
        return 2;
    }
    const char *dir = argv[2];
    int frames = 30;
    for (int i = 3; i + 1 < argc; i += 2)
        if (!strcmp(argv[i], "--frames")) frames = atoi(argv[i + 1]);
    if (frames <= 0) frames = 30;

    char path[1024];
    char *state = NULL, *unused = NULL;
    snprintf(path, sizeof(path), "%s/world_state.json", dir);
    if (read_file(path, &state)) {
        fprintf(stderr, "[littcli] missing %s\n", path);
        return 1;
    }

    LvSession s;
    snprintf(path, sizeof(path), "%s/assets/scenes/world.lscn.json", dir);
    {
        /* need the scene text twice: create() reads it itself */
        char scene_path[1024];
        snprintf(scene_path, sizeof(scene_path), "%s", path);
        char models_dir[1024];
        snprintf(models_dir, sizeof(models_dir), "%s/assets/models", dir);
        if (lv_session_create(state, scene_path, models_dir, &s)) {
            fprintf(stderr, "[littcli] failed to build session from %s\n",
                    scene_path);
            return 1;
        }
    }
    free(state);

    float nan_check = 0.0f;
    for (int i = 0; i < frames; i++) {
        lv_step(&s, 1.0f / 60.0f, 0.0f, 0.0f, 0);
        nan_check += fabsf(s.pos[0]) + fabsf(s.pos[1]) + fabsf(s.pos[2]);
    }

    int interactives = s.ent_count;
    int ok = interactives > 0 && s.missing_models == 0 &&
             nan_check == nan_check; /* NaN guard */

    printf("[native] rendered %d frames | %ld tris | %d solids | %d interactives\n",
           frames, s.tri_count, s.solid_count, interactives);
    printf("{\"ok\":%s,\"mode\":\"%s\",\"frames\":%d,\"solids\":%d,"
           "\"interactives\":%d,\"tris\":%ld,\"missing\":%d}\n",
           ok ? "true" : "false", lv_mode_name(s.cfg.mode), frames,
           s.solid_count, interactives, s.tri_count, s.missing_models);
    free(unused);
    lv_session_free(&s);
    return ok ? 0 : 1;
}
