/* litt_world.c - config parse + contract sim (port of gameplay.rs). */
#include "litt_world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _MSC_VER
#define strncasecmp _strnicmp   /* n9: no POSIX strncasecmp under MSVC */
#endif

#define KILL_PLANE_Y (-14.0f)
#define DEAD_FREEZE_S 0.7f
#define ENEMY_SPEED 3.2f
#define PLAYER_R 0.45f
#define HEAD_H 1.8f

static int has_tag(const LvJson *tags, const char *t) {
    if (!tags || tags->kind != LJ_ARR) return 0;
    for (int i = 0; i < tags->count; i++) {
        const char *s = lvj_str(lvj_at(tags, i), NULL);
        if (s && !strcmp(s, t)) return 1;
    }
    return 0;
}

static const char *has_model(const LvJson *tags) {
    if (!tags || tags->kind != LJ_ARR) return NULL;
    for (int i = 0; i < tags->count; i++) {
        const char *s = lvj_str(lvj_at(tags, i), NULL);
        if (s && !strncmp(s, "model:", 6)) return s + 6;
    }
    return NULL;
}

const char *lv_mode_name(LvMode m) {
    switch (m) {
    case LV_MODE_TOP: return "TopDown";
    case LV_MODE_2D5: return "Side2D5";
    default: return "Orbit3D";
    }
}

static void resolve_mode(const char *movement, const char *camera, LvConfig *c) {
    /* runtime contract (gameplay.rs:310-318): platformer movement or side
     * camera -> 2D5; top_down/isometric CAMERA -> TOP; else 3D orbit. */
    if (strstr(movement, "platformer") || strstr(camera, "side"))
        c->mode = LV_MODE_2D5;
    else if (strstr(camera, "top_down") || strstr(camera, "isometric"))
        c->mode = LV_MODE_TOP;
    else
        c->mode = LV_MODE_3D;
}

/* n12 belt-and-braces: a finite stored number or the default. The scanner
 * already rejects non-finite tokens; this guards future callers. */
static double jnum(const LvJson *v, double def) {
    return (v && v->kind == LJ_NUM && isfinite(v->num)) ? v->num : def;
}

int lv_config_from_state(const char *text, LvConfig *out) {
    memset(out, 0, sizeof(*out));
    /* defaults mirror the reference runtime */
    out->gravity = 22.0f;
    out->jump_v = 8.0f;
    out->run_speed = 7.0f;
    out->coyote = 0.1f;
    out->buffer = 0.12f;
    out->enemy_aggro = 6.0f;
    out->kill_m = 1.1f;
    out->interact_m = 1.6f;
    out->lives = -1;
    out->coins_value = 10;
    out->mode = LV_MODE_3D;
    snprintf(out->objective, sizeof(out->objective), "%s", "explore the world");

    LvJson *root = lvj_parse_strict(text);   /* n10: validate path rejects garbage */
    if (!root) return 1;

    const LvJson *id = lvj_get(root, "identity");
    if (id) {
        const char *mv = lvj_str(lvj_get(id, "movement"), "");
        const char *cam = lvj_str(lvj_get(id, "camera"), "");
        resolve_mode(mv, cam, out);
        snprintf(out->identity, sizeof(out->identity), "%s", mv);
    }

    const LvJson *gp = lvj_get(root, "gameplay");
    if (gp) {
        const LvJson *ph = lvj_get(gp, "physics");
        if (ph) {
            out->gravity = (float)jnum(lvj_get(ph, "gravity"), out->gravity);
            out->jump_v = (float)jnum(lvj_get(ph, "jump_velocity"), out->jump_v);
            out->run_speed = (float)jnum(lvj_get(ph, "run_speed"), out->run_speed);
            out->coyote = (float)jnum(lvj_get(ph, "coyote_time_s"), out->coyote);
            /* m1: presence test, not sign test - an explicit 0 disables
             * buffering exactly as authored. */
            const LvJson *jbv = lvj_get(ph, "jump_buffer_s");
            out->buffer = (jbv && jbv->kind == LJ_NUM)
                              ? (float)jnum(jbv, out->buffer)
                              : out->coyote + 0.02f;
        }
        out->enemy_aggro = (float)jnum(lvj_get(gp, "enemy_aggro_m"), out->enemy_aggro);
        out->kill_m = (float)jnum(lvj_get(gp, "kill_radius_m"), out->kill_m);
        out->interact_m = (float)jnum(lvj_get(gp, "interact_radius_m"), out->interact_m);
        double lives = jnum(lvj_get(gp, "lives"), 0.0);
        if (lives > 0) out->lives = (int)lives;
        double goal = jnum(lvj_get(gp, "score_goal"), 0.0);
        if (goal > 0) out->score_goal = (unsigned)goal;
        out->corpse_run = lvj_bool(lvj_get(gp, "corpse_run"), 0);
        const LvJson *sc = lvj_get(gp, "scoring");
        if (sc && lvj_bool(lvj_get(sc, "coins"), 0)) out->coins_value = 25;
        const char *obj = lvj_str(lvj_get(gp, "objective"), NULL);
        if (obj) snprintf(out->objective, sizeof(out->objective), "%s", obj);
    } else {
        /* some states keep physics at top level */
        const LvJson *ph = lvj_get(root, "physics");
        if (ph) {
            out->gravity = (float)jnum(lvj_get(ph, "gravity"), out->gravity);
            out->jump_v = (float)jnum(lvj_get(ph, "jump_velocity"), out->jump_v);
            out->run_speed = (float)jnum(lvj_get(ph, "run_speed"), out->run_speed);
            out->coyote = (float)jnum(lvj_get(ph, "coyote_time_s"), out->coyote);
            /* m1: same presence test at top level */
            const LvJson *jbv = lvj_get(ph, "jump_buffer_s");
            out->buffer = (jbv && jbv->kind == LJ_NUM)
                              ? (float)jnum(jbv, out->buffer)
                              : out->coyote + 0.02f;
        }
    }

    lvj_free(root);
    return 0;
}

static int ent_push(LvSession *s, LvEnt e) {
    if (s->ent_count == s->ent_cap) {
        int nc = s->ent_cap ? s->ent_cap * 2 : 32;
        LvEnt *ne = realloc(s->ents, sizeof(LvEnt) * (size_t)nc);
        if (!ne) return 1;               /* m4 */
        s->ents = ne;
        s->ent_cap = nc;
    }
    s->ents[s->ent_count++] = e;
    return 0;
}

static int solid_push(LvSession *s, LvAabb a) {
    LvAabb *ns = realloc(s->solids, sizeof(LvAabb) * (size_t)(s->solid_count + 1));
    if (!ns) return 1;
    s->solids = ns;
    s->solids[s->solid_count++] = a;
    return 0;
}

/* world-space AABB of one model: union of mesh bounds scaled+translated.
 * n11: per-axis scale; m5: negative scale orders min/max so mirrored
 * nodes cannot produce inside-out solids. */
static int model_aabb(const char *models_dir, const char *name,
                      const float scale[3], const float pos[3],
                      LvAabb *out, long *tris) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.obj", models_dir, name);
    LvModel m;
    if (lv_obj_load(path, &m)) return 1;
    for (int k = 0; k < 3; k++) { out->min[k] = 1e9f; out->max[k] = -1e9f; }
    for (int i = 0; i < m.count; i++) {
        LvMesh *me = &m.meshes[i];
        *tris += me->in / 3;
        for (int k = 0; k < 3; k++) {
            float p0 = me->bmin[k] * scale[k] + pos[k];
            float p1 = me->bmax[k] * scale[k] + pos[k];
            float lo = p0 < p1 ? p0 : p1;
            float hi = p0 < p1 ? p1 : p0;
            if (lo < out->min[k]) out->min[k] = lo;
            if (hi > out->max[k]) out->max[k] = hi;
        }
    }
    lv_model_free(&m);
    return 0;
}

int lv_session_create(const char *state_text, const char *scene_path,
                      const char *models_dir, LvSession *out) {
    memset(out, 0, sizeof(*out));
    if (lv_config_from_state(state_text, &out->cfg)) return 1;

    FILE *f = fopen(scene_path, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 1; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 1; }   /* m4 */
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = 0;

    LvJson *scene = lvj_parse_strict(buf);   /* n10: validate path strict */
    free(buf);
    if (!scene) return 1;

    out->spawn[0] = 0; out->spawn[1] = 1.2f; out->spawn[2] = 4;
    const LvJson *nodes = lvj_get(scene, "nodes");
    int have_spawn = 0;
    int oom = 0;                         /* m4 */

    if (nodes && nodes->kind == LJ_ARR) {
        for (int i = 0; i < nodes->count; i++) {
            const LvJson *n = lvj_at(nodes, i);
            if (!n || n->kind != LJ_OBJ) continue;
            if (!lvj_bool(lvj_get(n, "visible"), 1)) continue;
            const char *name = lvj_str(lvj_get(n, "name"), "?");
            const LvJson *tags = lvj_get(n, "tags");
            float pos[3] = { 0, 0, 0 }, scl[3] = { 1, 1, 1 };
            lvj_arr_f3(lvj_get(n, "position"), pos);
            const LvJson *sj = lvj_get(n, "scale");
            if (sj && sj->kind == LJ_ARR)
                for (int k = 0; k < 3 && k < sj->count; k++)   /* n11: all axes */
                    scl[k] = (float)jnum(lvj_at(sj, k), 1.0);

            /* spawn */
            if (!have_spawn && has_tag(tags, "player")) {
                out->spawn[0] = pos[0];
                out->spawn[1] = pos[1] + 1.2f;
                out->spawn[2] = pos[2];
                have_spawn = 1;
                continue;
            }

            unsigned flags = 0;
            if (has_tag(tags, "enemy")) flags |= LV_F_ENEMY;
            if (has_tag(tags, "hazard")) flags |= LV_F_HAZARD;
            if (has_tag(tags, "pickup") || has_tag(tags, "score") ||
                has_tag(tags, "token") || has_tag(tags, "dice") ||
                has_tag(tags, "objective") || has_tag(tags, "scoring"))
                flags |= LV_F_SCORING;
            if (has_tag(tags, "goal") || has_tag(tags, "win")) flags |= LV_F_GOAL;
            if (has_tag(tags, "checkpoint")) flags |= LV_F_CHECKPOINT;
            if (has_tag(tags, "poi")) flags |= LV_F_POI;

            /* solids from walkable-tagged model nodes */
            const char *mdl = has_model(tags);
            if ((has_tag(tags, "floor") || has_tag(tags, "level") ||
                 has_tag(tags, "board") || has_tag(tags, "track") ||
                 has_tag(tags, "hub") || has_tag(tags, "terrain")) && mdl) {
                LvAabb a;
                long tris = 0;
                if (!model_aabb(models_dir, mdl, scl, pos, &a, &tris)) {
                    if (!oom && solid_push(out, a)) oom = 1;   /* m4 */
                    out->tri_count += tris;
                } else {
                    out->missing_models++;
                }
            }

            if (flags) {
                LvEnt e;
                memset(&e, 0, sizeof(e));
                snprintf(e.name, sizeof(e.name), "%s", name);
                float lift = (flags & LV_F_ENEMY) ? 0.0f : 0.5f;
                e.pos[0] = pos[0];
                e.pos[1] = pos[1] + lift;
                e.pos[2] = pos[2];
                e.base_y = pos[1];
                e.flags = flags;
                e.alive = 1;
                if (flags & LV_F_ENEMY) {
                    const char *ln = name;
                    if (!strncasecmp(ln, "boss_", 5) || strstr(ln, "boss"))
                        e.tier = LV_TIER_BOSS;
                    else if (!strncasecmp(ln, "elite_", 6))
                        e.tier = LV_TIER_ELITE;
                    else if (mdl && (!strcmp(mdl, "brute") || !strcmp(mdl, "knight")))
                        e.tier = LV_TIER_ELITE;
                    else
                        e.tier = LV_TIER_MOOK;
                }
                if (!oom && ent_push(out, e)) oom = 1;   /* m4 */
            }
        }
    }

    lvj_free(scene);

    if (oom) {                           /* m4: clean error, nothing leaked */
        lv_session_free(out);
        memset(out, 0, sizeof(*out));
        return 1;
    }

    out->pos[0] = out->spawn[0];
    out->pos[1] = out->spawn[1];
    out->pos[2] = out->spawn[2];
    out->lives_left = out->cfg.lives > 0 ? (unsigned)out->cfg.lives : 0;
    out->grounded = 0;
    out->scene_dirty = 1;
    return 0;
}

void lv_session_free(LvSession *s) {
    if (!s) return;
    free(s->ents);
    free(s->solids);
    s->ents = NULL;
    s->solids = NULL;
}

static float ground_at(const LvSession *s, float x, float z) {
    float best = -1000.0f;
    for (int i = 0; i < s->solid_count; i++) {
        const LvAabb *a = &s->solids[i];
        if (x >= a->min[0] - 0.3f && x <= a->max[0] + 0.3f &&
            z >= a->min[2] - 0.3f && z <= a->max[2] + 0.3f &&
            a->max[1] <= s->pos[1] + 0.6f && a->max[1] > best)
            best = a->max[1];
    }
    return best;
}

static void collide_walls(LvSession *s) {
    for (int i = 0; i < s->solid_count; i++) {
        const LvAabb *a = &s->solids[i];
        if (s->pos[1] + 0.1f >= a->max[1]) continue;      /* above top */
        if (s->pos[1] + HEAD_H <= a->min[1]) continue;    /* below */
        if (s->pos[0] < a->min[0] - PLAYER_R || s->pos[0] > a->max[0] + PLAYER_R ||
            s->pos[2] < a->min[2] - PLAYER_R || s->pos[2] > a->max[2] + PLAYER_R)
            continue;
        float dxmin = s->pos[0] - (a->min[0] - PLAYER_R);
        float dxmax = (a->max[0] + PLAYER_R) - s->pos[0];
        float dzmin = s->pos[2] - (a->min[2] - PLAYER_R);
        float dzmax = (a->max[2] + PLAYER_R) - s->pos[2];
        float m = dxmin;
        int axis = 0, sign = -1;
        if (dxmax < m) { m = dxmax; axis = 0; sign = 1; }
        if (dzmin < m) { m = dzmin; axis = 2; sign = -1; }
        if (dzmax < m) { m = dzmax; axis = 2; sign = 1; }
        s->pos[axis] += sign * m;
        s->vel[axis] = 0;
    }
    /* ceiling */
    for (int i = 0; i < s->solid_count; i++) {
        const LvAabb *a = &s->solids[i];
        if (s->pos[0] >= a->min[0] - PLAYER_R && s->pos[0] <= a->max[0] + PLAYER_R &&
            s->pos[2] >= a->min[2] - PLAYER_R && s->pos[2] <= a->max[2] + PLAYER_R &&
            s->pos[1] + HEAD_H > a->min[1] && s->pos[1] < a->min[1]) {
            s->pos[1] = a->min[1] - (HEAD_H + 0.05f);
            if (s->vel[1] > 0) s->vel[1] = 0;
        }
    }
}

static void sweep(LvSession *s, float dt) {
    s->any_chasing = 0;
    for (int i = 0; i < s->ent_count; i++) {
        LvEnt *e = &s->ents[i];
        if (!e->alive) continue;
        float dx = e->pos[0] - s->pos[0];
        float dz = e->pos[2] - s->pos[2];
        float hd = sqrtf(dx * dx + dz * dz);
        if (hd < 1e-4f) hd = 1e-4f;
        float dy = e->pos[1] - s->pos[1];

        if (e->flags & LV_F_ENEMY) {
            float d3 = sqrtf(hd * hd + dy * dy);
            if (d3 < s->cfg.enemy_aggro && d3 > 0.1f) {
                s->any_chasing = 1;
                float mul = e->tier == LV_TIER_BOSS ? 1.35f :
                            e->tier == LV_TIER_ELITE ? 1.15f : 1.0f;
                float lunge = 1.0f;
                if (e->tier == LV_TIER_BOSS) {
                    e->lunge_cd -= dt;
                    if (e->lunge_cd <= 0.0f && hd < 8.0f) {
                        lunge = 2.4f;
                        e->lunge_cd = 3.5f;
                    }
                }
                float sp = ENEMY_SPEED * mul * lunge;
                float nx = e->pos[0] - dx / hd * sp * dt;
                float nz = e->pos[2] - dz / hd * sp * dt;
                e->pos[0] = nx;
                e->pos[2] = nz;
                s->scene_dirty = 1;
                e->pos[1] += (e->base_y - e->pos[1]) * fminf(dt * 2.0f, 1.0f);
            }
            if (hd < s->cfg.kill_m + 0.4f && fabsf(dy) < 2.5f) {
                if (s->dead_t <= 0.0f) {
                    s->dead_t = DEAD_FREEZE_S;
                    if (s->cfg.lives > 0) {
                        s->lives_left = s->lives_left ? s->lives_left - 1 : 0;
                        if (s->lives_left == 0) s->game_over = 1;
                    }
                }
            }
            continue;
        }

        float d = sqrtf(hd * hd + dy * dy);
        if (d > s->cfg.interact_m) continue;
        if (e->flags & LV_F_HAZARD) {
            if (s->dead_t <= 0.0f) {
                s->dead_t = DEAD_FREEZE_S;
                if (s->cfg.lives > 0) {
                    s->lives_left = s->lives_left ? s->lives_left - 1 : 0;
                    if (s->lives_left == 0) s->game_over = 1;
                }
            }
        } else if (e->flags & LV_F_SCORING) {
            e->alive = 0;
            s->score += s->cfg.coins_value;
            s->scene_dirty = 1;
        } else if (e->flags & LV_F_GOAL) {
            s->won = 1;
        } else if (e->flags & LV_F_CHECKPOINT) {
            e->alive = 0;
            s->spawn[0] = e->pos[0];
            s->spawn[1] = e->pos[1] + 1.2f;
            s->spawn[2] = e->pos[2];
            s->scene_dirty = 1;
        } else if ((e->flags & LV_F_POI) && !e->seen_poi) {
            e->seen_poi = 1;
        }
    }
}

void lv_step(LvSession *s, float dt, float f, float saxis, int jump_pressed) {
    s->now += dt;
    s->anim_t += dt;

    if (s->game_over || s->won) return;
    if (s->dead_t > 0.0f) {
        s->dead_t -= dt;
        if (s->dead_t <= 0.0f) {
            s->pos[0] = s->spawn[0];
            s->pos[1] = s->spawn[1];
            s->pos[2] = s->spawn[2];
            s->vel[0] = s->vel[1] = s->vel[2] = 0;
            /* n5: stale jump state must not survive a teleport */
            s->grounded = 0;
            s->coyote_t = 0;
            s->buf_t = 0;
        }
        return;
    }

    /* coyote + buffer */
    if (s->grounded) s->coyote_t = s->cfg.coyote;
    else s->coyote_t -= dt;
    if (jump_pressed) s->buf_t = s->cfg.buffer;
    else s->buf_t -= dt;

    if (s->buf_t > 0.0f && s->coyote_t > 0.0f) {
        s->vel[1] = s->cfg.jump_v;
        s->buf_t = 0;
        s->coyote_t = 0;
        s->grounded = 0;
    }

    /* contract input mapping */
    float vx = 0, vz = 0;
    if (s->cfg.mode == LV_MODE_TOP) {
        vx = saxis * s->cfg.run_speed;
        vz = -f * s->cfg.run_speed;
        s->pos[0] += vx * dt;
        s->pos[2] += vz * dt;
    } else if (s->cfg.mode == LV_MODE_2D5) {
        vx = saxis * s->cfg.run_speed;
        s->pos[0] += vx * dt;
    } else {
        /* camera-relative on the plane (yaw fixed headless) */
        float sy = sinf(s->yaw), cy = cosf(s->yaw);
        vx = (saxis * cy + f * sy) * s->cfg.run_speed;
        vz = (-saxis * sy + f * cy) * s->cfg.run_speed;
        s->pos[0] += vx * dt;
        s->pos[2] += vz * dt;
    }

    /* gravity + ground snap */
    s->vel[1] -= s->cfg.gravity * dt;
    s->pos[1] += s->vel[1] * dt;
    float gy = ground_at(s, s->pos[0], s->pos[2]);
    if (gy > -900.0f && s->pos[1] <= gy) {
        s->pos[1] = gy;
        s->vel[1] = 0;
        s->grounded = 1;
    } else {
        s->grounded = 0;
    }

    collide_walls(s);

    if (s->pos[1] < KILL_PLANE_Y) {
        s->dead_t = DEAD_FREEZE_S;
        if (s->cfg.lives > 0) {
            s->lives_left = s->lives_left ? s->lives_left - 1 : 0;
            if (s->lives_left == 0) s->game_over = 1;
        }
        s->pos[0] = s->spawn[0];
        s->pos[1] = s->spawn[1];
        s->pos[2] = s->spawn[2];
        s->vel[1] = 0;
        /* n5: same stale-timer reset as the freeze-expiry teleport */
        s->grounded = 0;
        s->coyote_t = 0;
        s->buf_t = 0;
    }

    sweep(s, dt);

    if (s->cfg.score_goal > 0 && s->score >= s->cfg.score_goal) s->won = 1;
}
