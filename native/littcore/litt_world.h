/* litt_world.h - world-state config + headless gameplay sim
 * (port of src/gameplay.rs contract). */
#ifndef LITT_WORLD_H
#define LITT_WORLD_H

#include "litt_json.h"
#include "litt_obj.h"

typedef enum { LV_MODE_3D = 0, LV_MODE_TOP, LV_MODE_2D5 } LvMode;
typedef enum { LV_TIER_MOOK = 0, LV_TIER_ELITE, LV_TIER_BOSS } LvTier;

enum {
    LV_F_ENEMY = 1 << 0,
    LV_F_HAZARD = 1 << 1,
    LV_F_SCORING = 1 << 2,
    LV_F_GOAL = 1 << 3,
    LV_F_CHECKPOINT = 1 << 4,
    LV_F_POI = 1 << 5,
};

typedef struct {
    LvMode mode;
    float gravity, jump_v, run_speed, coyote, buffer;
    float enemy_aggro, kill_m, interact_m;
    int lives;              /* -1 = infinite */
    unsigned score_goal;    /* 0 = none */
    unsigned coins_value;
    int corpse_run;
    char objective[192];
    char identity[96];      /* movement substring for diagnostics */
} LvConfig;

typedef struct {
    char name[96];
    float pos[3];
    float base_y;
    unsigned flags;
    LvTier tier;
    float lunge_cd;
    int alive, seen_poi;
} LvEnt;

typedef struct { float min[3], max[3]; } LvAabb;

typedef struct {
    LvConfig cfg;
    float pos[3], vel[3], spawn[3];
    float yaw;                 /* 3D camera-relative movement */
    int grounded, won, game_over;
    unsigned score, lives_left;
    float now, anim_t, coyote_t, buf_t, dead_t;
    int scene_dirty, any_chasing;
    LvEnt *ents;
    int ent_count;
    int ent_cap;
    LvAabb *solids;
    int solid_count;
    long tri_count;            /* triangles referenced by solids (info) */
    int missing_models;
} LvSession;

/* Parse config from raw world_state.json text. Always yields a usable cfg. */
int lv_config_from_state(const char *state_text, LvConfig *out);

/* Build a session: parses scene lscn at scene_path, loads model OBJs from
 * models_dir for walkable-tagged nodes. Returns 0 on success. */
int lv_session_create(const char *state_text, const char *scene_path,
                      const char *models_dir, LvSession *out);
void lv_session_free(LvSession *s);

/* One simulation tick. f = W(+)/S(-), s = D(+)/A(-), jump_pressed edge. */
void lv_step(LvSession *s, float dt, float f, float saxis, int jump_pressed);

const char *lv_mode_name(LvMode m);

#endif
