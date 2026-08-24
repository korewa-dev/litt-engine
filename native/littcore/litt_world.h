// LittWorld - Game world simulation
// Player state, entities, physics, win/lose
//
// This header carries BOTH halves of the staged rewrite:
//   1. the C11 contract (LvConfig/LvSession/lv_*) consumed by the C core
//      (litt_world.c, littcli.c, tests.c) - restored verbatim from git
//      history after the C++ wave clobbered it;
//   2. the C++ WorldManager used by game.cpp / litteditor.
#pragma once

/* ------------------------------------------------------------------ */
/* C contract (port of src/gameplay.rs)                                */
/* ------------------------------------------------------------------ */
#include "litt_json.h"
#include "litt_obj.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

/* ------------------------------------------------------------------ */
/* C++ world manager                                                   */
/* ------------------------------------------------------------------ */
#ifdef __cplusplus
#include "litt_math.h"
#include "litt_input.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace litt {

enum class Mode { Mode3D = 0, ModeTop, Mode2D5 };
enum class Tier { Mook = 0, Elite, Boss };

struct Config {
    Mode mode = Mode::Mode3D;
    float gravity = -9.81f;
    float jump = 8.0f;
    float speed = 5.0f;
    float coyote = 0.1f;
    float buffer = 0.1f;
    float aggro = 10.0f;
    float kill_dist = 2.0f;
    int lives = -1;
    unsigned score_goal = 0;
    char objective[128] = "";
};

// World entity (game object)
struct WorldEntity {
    char name[64] = "";
    Vec3 pos = Vec3::zero();
    unsigned flags = 0;
    Tier tier = Tier::Mook;
    int alive = 1;
    enum Flags { Enemy = 1<<0, Hazard = 1<<1, Scoring = 1<<2, Goal = 1<<3, Checkpoint = 1<<4 };
};

struct WorldState {
    Config cfg;
    Vec3 pos = Vec3::zero();
    Vec3 vel = Vec3::zero();
    float yaw = 0.0f;
    int grounded = 0;
    int won = 0;
    int game_over = 0;
    unsigned score = 0;
    unsigned lives = 0;
    float now = 0.0f;
    float anim = 0.0f;
    std::vector<WorldEntity> ents;
    std::vector<Aabb> solids;
};

class WorldManager {
public:
    WorldState state;
    
    bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        return parse_json(content);
    }
    
    bool save(const std::string& path) {
        std::ofstream f(path);
        if (!f.is_open()) return false;
        f << serialize();
        return true;
    }
    
    void update(float dt) {
        state.now += dt;
        state.anim += dt;
        
        // Gravity
        state.vel.y += state.cfg.gravity * dt;
        state.pos += state.vel * dt;
        
        // Ground
        if (state.pos.y <= 0) {
            state.pos.y = 0;
            state.vel.y = 0;
            state.grounded = 1;
        } else {
            state.grounded = 0;
        }
        
        // Entities
        for (auto& e : state.ents) {
            if (!e.alive) continue;
            float dist = (e.pos - state.pos).length();
            if ((e.flags & WorldEntity::Enemy) && dist < state.cfg.aggro) {
                Vec3 dir = (state.pos - e.pos).normalized();
                e.pos += dir * dt * 2.0f;
            }
            if ((e.flags & WorldEntity::Enemy) && dist < state.cfg.kill_dist) {
                e.alive = 0;
                if (state.lives > 0) state.lives--;
            }
        }
        
        // Goals
        for (const auto& e : state.ents) {
            if (e.flags & WorldEntity::Goal) {
                if ((e.pos - state.pos).length() < 3.0f) state.won = 1;
            }
        }
        
        // Lose
        if (state.lives <= 0) state.game_over = 1;
    }
    
    void input(const Input& in) {
        Vec3 move(0, 0, 0);
        if (in.key_down(Key::W) || in.key_down(Key::Up)) move.z -= 1;
        if (in.key_down(Key::S) || in.key_down(Key::Down)) move.z += 1;
        if (in.key_down(Key::A) || in.key_down(Key::Left)) move.x -= 1;
        if (in.key_down(Key::D) || in.key_down(Key::Right)) move.x += 1;
        
        if (move.length() > 0) {
            move = move.normalized();
            float c = std::cos(state.yaw), s = std::sin(state.yaw);
            state.pos.x += (move.x * c - move.z * s) * state.cfg.speed * 0.016f;
            state.pos.z += (move.x * s + move.z * c) * state.cfg.speed * 0.016f;
        }
        
        if (in.action_pressed("jump") && state.grounded) {
            state.vel.y = state.cfg.jump;
            state.grounded = 0;
        }
    }
    
    std::string serialize() const {
        std::ostringstream o;
        o << "{\"pos\":[" << state.pos.x << "," << state.pos.y << "," << state.pos.z << "],"
          << "\"score\":" << state.score << ",\"lives\":" << state.lives << "}";
        return o.str();
    }
    
private:
    bool parse_json(const std::string& s) {
        // Simple JSON parser
        return false;
    }
};

} // namespace litt
#endif
