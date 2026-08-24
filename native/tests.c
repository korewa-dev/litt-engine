/* tests.c - unit tests for littcore. Build+run via native/build.bat test
 * or `make -C native test`. Tiny assert harness, no deps. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "littcore/litt_json.h"
#include "littcore/litt_obj.h"
#include "littcore/litt_world.h"

static int failures = 0;
#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (cond) printf("ok  %s\n", msg);                            \
        else { printf("FAIL %s (%s:%d)\n", msg, __FILE__, __LINE__);   \
               failures++; }                                          \
    } while (0)

static void write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    fputs(text, f);
    fclose(f);
}

int main(void) {
    /* ---- json scanner ---- */
    {
        const char *doc =
            "{\"a\":{\"b\":[10,-2.5,\"x\"],\"c\":true},\"d\":null}";
        LvJson *v = lvj_parse(doc);
        CHECK(v != NULL, "json: parses nested doc");
        double n = lvj_num(lvj_at(lvj_get(lvj_get(v, "a"), "b"), 1), 0);
        CHECK(fabs(n + 2.5) < 1e-9, "json: negative float round-trips");
        CHECK(lvj_bool(lvj_get(lvj_get(v, "a"), "c"), 0) == 1,
              "json: true survives");
        CHECK(lvj_num(lvj_get(v, "missing"), 42.0) == 42.0,
              "json: missing key returns default");
        lvj_free(v);
        CHECK(lvj_parse("{broken") == NULL, "json: rejects broken doc");
    }

    /* ---- obj loader: groups become named meshes ---- */
    {
        remove("t_rig.obj");
        write_file("t_rig.obj",
            "g knight_torso\nusemtl prop_metal\n"
            "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1//1 2//1 3//1\n"
            "g knight_leg_l\nusemtl prop_metal_dk\n"
            "v 5 0 0\nv 6 0 0\nv 5 1 0\nf 4//2 5//2 6//2\n");
        LvModel m;
        CHECK(lv_obj_load("t_rig.obj", &m) == 0, "obj: loads rig file");
        CHECK(m.count == 2, "obj: each g-group is its own mesh");
        if (m.count == 2) {
            CHECK(!strcmp(m.meshes[0].name, "knight_torso") &&
                  !strcmp(m.meshes[1].name, "knight_leg_l"),
                  "obj: part names survive");
            CHECK(m.meshes[1].idx[0] == 0 && m.meshes[1].vn == 3,
                  "obj: per-group index remap from zero");
            CHECK(m.meshes[1].bmin[0] > 4.9f && m.meshes[1].bmax[0] < 6.1f,
                  "obj: bounds computed");
        }
        lv_model_free(&m);
        remove("t_rig.obj");
    }

    /* ---- world: mode resolution + physics defaults ---- */
    {
        LvConfig c;
        lv_config_from_state("{\"identity\":{\"movement\":\"top_down\",\"camera\":\"top\"}}", &c);
        CHECK(c.mode == LV_MODE_TOP && fabs(c.gravity - 22.0f) < 1e-5 &&
              fabs(c.buffer - 0.12f) < 1e-5,
              "world: TOP mode + default physics");
        lv_config_from_state("{\"identity\":{\"movement\":\"side_scrolling_2_5d\"}}", &c);
        CHECK(c.mode == LV_MODE_2D5, "world: 2_5d substring -> Side2D5");

        char st[512];
        snprintf(st, sizeof(st),
            "{\"gameplay\":{\"physics\":{\"gravity\":30,"
            "\"jump_buffer_s\":0.14,\"coyote_time_s\":0.12},\"lives\":3,"
            "\"scoring\":{\"coins\":true}}}");
        lv_config_from_state(st, &c);
        CHECK(fabs(c.gravity - 30.0f) < 1e-5 &&
              fabs(c.buffer - 0.14f) < 1e-5 && c.lives == 3 &&
              c.coins_value == 25,
              "world: gameplay block parses all contract fields");
    }

    /* ---- world sim on a synthetic scene ---- */
    {
        /* tiny model for the floor solid */
        write_file("t_floor.obj",
            "g pad\nv -4 0 -4\nv 4 0 -4\nv 4 0 4\nv -4 0 4\n"
            "f 1 2 3\nf 1 3 4\n");
        write_file("t_scene.json",
            "{\"nodes\":["
            "{\"name\":\"Root\",\"position\":[0,0,0],\"scale\":[1,1,1],\"visible\":true,\"tags\":[]},"
            "{\"name\":\"Pad\",\"position\":[0,0,0],\"scale\":[1,1,1],\"visible\":true,"
            " \"tags\":[\"floor\",\"model:t_floor\"]},"
            "{\"name\":\"Coin_01\",\"position\":[0.5,0.5,0.5],\"scale\":[1,1,1],\"visible\":true,"
            " \"tags\":[\"pickup\",\"model:coin\"]},"
            "{\"name\":\"Boss_X\",\"position\":[6,1.2,6],\"scale\":[1,1,1],\"visible\":true,"
            " \"tags\":[\"enemy\",\"model:knight\"]}"
            "]}");
        char state[256];
        snprintf(state, sizeof(state),
            "{\"identity\":{\"movement\":\"top_down\"},"
            "\"gameplay\":{\"physics\":{\"gravity\":22},\"lives\":3}}");

        LvSession s;
        int rc = lv_session_create(state, "t_scene.json", ".", &s);
        CHECK(rc == 0, "world: session builds");
        CHECK(s.solid_count == 1, "world: walkable tag becomes solid");
        CHECK(s.ent_count == 2, "world: coin + boss registered");
        if (s.ent_count == 2)
            CHECK(s.ents[1].tier == LV_TIER_BOSS,
                  "world: heavy-model enemy reads as boss tier");

        /* gravity pulls down then lands on the pad */
        for (int i = 0; i < 60; i++) lv_step(&s, 1.0f / 60.0f, 0, 0, 0);
        CHECK(fabsf(s.pos[1] - s.solids[0].max[1]) < 0.05f,
              "world: lands on ground plane");
        CHECK(s.grounded, "world: grounded flag set after landing");

        /* TOP W pushes away (-z), D strafes (+x) */
        float z0 = s.pos[2];
        lv_step(&s, 1.0f / 60.0f, 1.0f, 0.0f, 0);
        CHECK(s.pos[2] < z0, "world: TOP W moves -z (away)");

        /* pickup consumption scores */
        unsigned sc0 = s.score;
        s.pos[0] = 0.5f; s.pos[2] = 0.5f; s.pos[1] = 1.0f;
        lv_step(&s, 1.0f / 60.0f, 0, 0, 0);
        CHECK(s.score > sc0, "world: pickup adds score");

        lv_session_free(&s);
        remove("t_scene.json");
        remove("t_floor.obj");
    }

    printf("\n%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
