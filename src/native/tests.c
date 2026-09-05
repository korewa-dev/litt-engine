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

/* n7: report fixture-write failure instead of NULL-fputs downstream */
static int write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (!f) return 1;
    int bad = fputs(text, f) < 0;
    if (fclose(f) != 0) bad = 1;
    return bad;
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

        /* m2: truncated literals are no longer accepted */
        CHECK(lvj_parse("tru") == NULL, "json: 'tru' rejected (m2)");
        CHECK(lvj_parse("fals") == NULL, "json: 'fals' rejected (m2)");
        CHECK(lvj_parse("nul") == NULL, "json: 'nul' rejected (m2)");
        LvJson *tv = lvj_parse("true");
        CHECK(tv != NULL && lvj_bool(tv, 0) == 1,
              "json: full 'true' still parses (m2)");
        lvj_free(tv);
        /* m3: strict number grammar */
        CHECK(lvj_parse("[+1]") == NULL, "json: leading '+' rejected (m3)");
        CHECK(lvj_parse("[.5]") == NULL, "json: bare '.5' rejected (m3)");
        CHECK(lvj_parse("[1.]") == NULL, "json: trailing '1.' rejected (m3)");
        CHECK(lvj_parse("[01]") == NULL, "json: leading zero rejected (m3)");
        CHECK(lvj_parse("[0x10]") == NULL, "json: hex extension rejected (m3)");
        CHECK(lvj_parse("[1e999]") == NULL,
              "json: strtod overflow rejected, not inf (m3+n12)");
        char big[96];
        big[0] = '[';
        for (int k = 1; k <= 80; k++) big[k] = '1';
        big[81] = ']';
        big[82] = 0;
        CHECK(lvj_parse(big) == NULL,
              "json: >63-char token rejected, not truncated (m3)");
        LvJson *ok_num = lvj_parse("[1e-3,0.5,-0]");
        CHECK(ok_num != NULL &&
              fabs(lvj_at(ok_num, 0)->num - 0.001) < 1e-12,
              "json: legal exponent form still parses (m3)");
        lvj_free(ok_num);
        /* n10: lenient vs strict trailing-garbage handling */
        CHECK(lvj_parse("{} junk") != NULL,
              "json: lenient parse tolerates trailing text (back-compat)");
        CHECK(lvj_parse_strict("{} junk") == NULL,
              "json: strict parse rejects trailing garbage (n10)");
        LvJson *st1 = lvj_parse_strict("{}");
        CHECK(st1 != NULL, "json: strict parse accepts exact doc (n10)");
        lvj_free(st1);
        LvJson *st2 = lvj_parse_strict("{\"a\":1}\n ");
        CHECK(st2 != NULL, "json: strict parse allows trailing ws (n10)");
        lvj_free(st2);
    }

    /* ---- obj loader: groups become named meshes ---- */
    if (write_file("t_rig.obj",
                   "g knight_torso\nusemtl prop_metal\n"
                   "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1//1 2//1 3//1\n"
                   "g knight_leg_l\nusemtl prop_metal_dk\n"
                   "v 5 0 0\nv 6 0 0\nv 5 1 0\nf 4//2 5//2 6//2\n")) {
        printf("FAIL io: cannot write t_rig.obj\n");
        failures++;
    } else {
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
    }
    remove("t_rig.obj");

    /* ---- obj robustness: negative indices wrap, tabs split (n4) ---- */
    {
        remove("t_neg.obj");
        if (write_file("t_neg.obj",
                       "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
                       "f -4 -3 -2\nf 2\t3\t4\n")) {
            printf("FAIL io: cannot write t_neg.obj\n");
            failures++;
        } else {
            LvModel m;
            int rc = lv_obj_load("t_neg.obj", &m);
            CHECK(rc == 0, "obj: negative indices wrap around (n4)");
            if (rc == 0) {
                CHECK(m.count == 1 && m.meshes[0].in == 6 &&
                      m.meshes[0].vn == 4,
                      "obj: wrapped + tab-separated faces both load (n4)");
                lv_model_free(&m);
            }
        }
        remove("t_neg.obj");

        remove("t_zero.obj");
        if (write_file("t_zero.obj",
                       "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 1 2\n")) {
            printf("FAIL io: cannot write t_zero.obj\n");
            failures++;
        } else {
            LvModel mz;
            CHECK(lv_obj_load("t_zero.obj", &mz) != 0,
                  "obj: index 0 drops the face -> clean error, no crash (n4)");
        }
        remove("t_zero.obj");
    }

    /* ---- world: mode resolution + physics defaults ---- */
    {
        LvConfig c;
        lv_config_from_state("{\"identity\":{\"movement\":\"free_roam\","
                             "\"camera\":\"isometric\"}}", &c);
        CHECK(c.mode == LV_MODE_TOP && fabs(c.gravity - 22.0f) < 1e-5 &&
              fabs(c.buffer - 0.12f) < 1e-5,
              "world: TOP mode + default physics");
        lv_config_from_state("{\"identity\":{\"movement\":\"platformer_movement\"}}", &c);
        CHECK(c.mode == LV_MODE_2D5, "world: platformer movement -> Side2D5");

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

        /* m1: explicit jump_buffer_s 0 is honored verbatim */
        lv_config_from_state("{\"gameplay\":{\"physics\":{"
                             "\"coyote_time_s\":0.3,\"jump_buffer_s\":0}}}", &c);
        CHECK(fabs(c.buffer) < 1e-6,
              "world: explicit jump_buffer_s 0 disables buffering (m1)");
        lv_config_from_state("{\"gameplay\":{\"physics\":{"
                             "\"coyote_time_s\":0.3}}}", &c);
        CHECK(fabs(c.buffer - 0.32f) < 1e-5,
              "world: absent jump_buffer_s defaults coyote+0.02 (m1)");
        lv_config_from_state("{\"physics\":{\"jump_buffer_s\":0}}", &c);
        CHECK(fabs(c.buffer) < 1e-6,
              "world: top-level physics block honors 0 buffer too (m1)");

        /* n10/n12: validate path rejects malformed documents outright */
        CHECK(lv_config_from_state("{\"identity\":{}} junk", &c) == 1,
              "world: trailing garbage fails config parse (n10)");
        CHECK(lv_config_from_state(
                  "{\"gameplay\":{\"physics\":{\"gravity\":1e999}}}", &c) == 1,
              "world: non-finite gravity rejected at parse (n12)");
    }

    /* ---- world sim on a synthetic scene ---- */
    {
        /* tiny model for the floor solid */
        int io = write_file("t_floor.obj",
            "g pad\nv -4 0 -4\nv 4 0 -4\nv 4 0 4\nv -4 0 4\n"
            "f 1 2 3\nf 1 3 4\n");
        io |= write_file("t_scene.json",
            "{\"nodes\":["
            "{\"name\":\"Root\",\"position\":[0,0,0],\"scale\":[1,1,1],\"visible\":true,\"tags\":[]},"
            "{\"name\":\"Pad\",\"position\":[0,0,0],\"scale\":[1,1,1],\"visible\":true,"
            " \"tags\":[\"floor\",\"model:t_floor\"]},"
            "{\"name\":\"Coin_01\",\"position\":[0.5,0.5,0.5],\"scale\":[1,1,1],\"visible\":true,"
            " \"tags\":[\"pickup\",\"model:coin\"]},"
            "{\"name\":\"Boss_X\",\"position\":[6,1.2,6],\"scale\":[1,1,1],\"visible\":true,"
            " \"tags\":[\"enemy\",\"model:knight\"]},"
            "{\"name\":\"mob_grunt\",\"position\":[-6,1.2,-6],\"scale\":[1,1,1],\"visible\":true,"
            " \"tags\":[\"enemy\",\"model:knight\"]}"
            "]}");
        if (io) {
            printf("FAIL io: cannot write t_floor.obj/t_scene.json\n");
            failures++;
        } else {
            char state[256];
            snprintf(state, sizeof(state),
                "{\"identity\":{\"camera\":\"top_down\"},"
                "\"gameplay\":{\"physics\":{\"gravity\":22},\"lives\":3}}");

            LvSession s;
            int rc = lv_session_create(state, "t_scene.json", ".", &s);
            CHECK(rc == 0, "world: session builds");
            CHECK(s.solid_count == 1, "world: walkable tag becomes solid");
            CHECK(s.ent_count == 3, "world: coin + boss + grunt registered");
            if (s.ent_count == 3) {
                CHECK(s.ents[1].tier == LV_TIER_BOSS,
                      "world: heavy-model enemy reads as boss tier");
                CHECK(s.ents[2].tier == LV_TIER_ELITE,
                      "world: knight-model grunt reads elite (n7)");
            }

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
        }
        remove("t_scene.json");
        remove("t_floor.obj");
    }

    /* ---- scale handling: per-axis scale honored, negative scale keeps
     * AABBs ordered (m5, n11) ---- */
    {
        int io = write_file("t_spad.obj",
            "g pad\nv -4 0 -4\nv 4 0 -4\nv 4 0 4\nv -4 0 4\n"
            "f 1 2 3\nf 1 3 4\n");
        io |= write_file("t_scn2.json",
            "{\"nodes\":["
            "{\"name\":\"Root\",\"position\":[0,0,0],\"visible\":true,\"tags\":[]},"
            "{\"name\":\"PadA\",\"position\":[20,0,0],\"scale\":[2,1,1],\"visible\":true,"
            " \"tags\":[\"floor\",\"model:t_spad\"]},"
            "{\"name\":\"PadN\",\"position\":[-20,0,0],\"scale\":[-1,1,1],\"visible\":true,"
            " \"tags\":[\"floor\",\"model:t_spad\"]}"
            "]}");
        if (io) {
            printf("FAIL io: cannot write t_spad.obj/t_scn2.json\n");
            failures++;
        } else {
            LvSession s;
            int rc = lv_session_create(
                "{\"identity\":{\"camera\":\"top_down\"}}",
                "t_scn2.json", ".", &s);
            CHECK(rc == 0 && s.solid_count == 2,
                  "scale: two scaled solids built (n11)");
            if (rc == 0 && s.solid_count == 2) {
                int ordered = 1;
                for (int i = 0; i < s.solid_count; i++)
                    for (int k = 0; k < 3; k++)
                        if (s.solids[i].min[k] > s.solids[i].max[k])
                            ordered = 0;
                CHECK(ordered,
                      "scale: mirrored node keeps min<=max per axis (m5)");
                LvAabb *a = &s.solids[0], *b = &s.solids[1];
                CHECK(a->min[0] > 11.9f && a->max[0] < 28.1f,
                      "scale: [2,1,1] doubles x extent only (n11)");
                CHECK(b->min[0] > -24.1f && b->max[0] < -15.9f &&
                      fabsf(b->max[0] - b->min[0] - 8.0f) < 0.05f,
                      "scale: [-1,1,1] mirrors without inverting (m5)");
                CHECK(s.tri_count == 4, "scale: tri accounting unchanged");
            }
            lv_session_free(&s);
        }
        remove("t_scn2.json");
        remove("t_spad.obj");
    }

    /* ---- respawn must drop stale coyote/buffer/grounded (n5) ----
     * Deterministic walk into a hazard: player lands (~f20), walks north
     * off the pad edge (~f37, coyote refreshed), enters the kill radius
     * (~f39) while coyote is still positive; the freeze preserves it.
     * On the respawn frame a pressed jump must NOT fire off stale state. */
    {
        int io = write_file("t_rpad.obj",
            "g pad\nv -4 0 -4\nv 4 0 -4\nv 4 0 4\nv -4 0 4\n"
            "f 1 2 3\nf 1 3 4\n");
        io |= write_file("t_rscn.json",
            "{\"nodes\":["
            "{\"name\":\"Root\",\"position\":[0,0,0],\"visible\":true,\"tags\":[]},"
            "{\"name\":\"Pad\",\"position\":[0,0,0],\"visible\":true,"
            " \"tags\":[\"floor\",\"model:t_rpad\"]},"
            "{\"name\":\"Spike_Row\",\"position\":[0,0,-6],\"visible\":true,"
            " \"tags\":[\"hazard\"]}"
            "]}");
        if (io) {
            printf("FAIL io: cannot write t_rpad.obj/t_rscn.json\n");
            failures++;
        } else {
            LvSession s;
            int rc = lv_session_create(
                "{\"identity\":{\"camera\":\"top_down\"},"
                "\"gameplay\":{\"physics\":{\"gravity\":22},\"lives\":3}}",
                "t_rscn.json", ".", &s);
            if (rc != 0) {
                CHECK(0, "respawn: session builds");
            } else {
                float pad_top = s.solids[0].max[1];
                int died = 0;
                for (int f = 1; f <= 200; f++) {           /* hold W, no jump */
                    lv_step(&s, 1.0f / 60.0f, 1.0f, 0.0f, 0);
                    if (s.dead_t > 0.0f) { died = 1; break; }
                }
                CHECK(died, "respawn: walking into hazard kills");
                for (int f = 0; f < 200 && s.dead_t > 0.0f; f++)
                    lv_step(&s, 1.0f / 60.0f, 0, 0, 0);
                CHECK(s.dead_t <= 0.0f && !s.game_over && s.lives_left == 2,
                      "respawn: freeze elapses, exactly one life lost");
                lv_step(&s, 1.0f / 60.0f, 0.0f, 0.0f, 1);  /* slam jump */
                CHECK(s.vel[1] < 0.0f,
                      "respawn: stale coyote/buffer cleared, no ghost jump (n5)");
                for (int f = 0; f < 30; f++)
                    lv_step(&s, 1.0f / 60.0f, 0, 0, 0);
                CHECK(s.pos[1] <= pad_top + 0.05f,
                      "respawn: settles back on the pad (n5)");
                lv_session_free(&s);
            }
        }
        remove("t_rscn.json");
        remove("t_rpad.obj");
    }

    /* ---- regressions: CORE_AUDIT C1 (OBJ no trailing newline),
     * M3 (astral-plane escapes), mode contract (M1/M2) ---- */
    {
        /* (a) OBJ whose last line has no trailing newline must not crash */
        remove("t_nonl.obj");
        if (write_file("t_nonl.obj",
                       "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3")) {
            printf("FAIL io: cannot write t_nonl.obj\n");
            failures++;
        } else {
            LvModel m;
            int rc = lv_obj_load("t_nonl.obj", &m);
            CHECK(rc == 0, "obj: no trailing newline does not crash");
            if (rc == 0) {
                CHECK(m.count == 1 && m.meshes[0].in == 3,
                      "obj: unterminated last line yields 1 face");
                lv_model_free(&m);
            }
        }
        remove("t_nonl.obj");

        /* (b) \ud83d\ude00 decodes to real 4-byte UTF-8, not mojibake */
        LvJson *j = lvj_parse("{\"e\":\"\\ud83d\\ude00\"}");
        const char *em = lvj_str(lvj_get(j, "e"), NULL);
        CHECK(em && (unsigned char)em[0] == 0xF0 && (unsigned char)em[1] == 0x9F &&
              (unsigned char)em[2] == 0x98 && (unsigned char)em[3] == 0x80 &&
              em[4] == 0,
              "json: \\ud83d\\ude00 -> F0 9F 98 80");
        lvj_free(j);

        /* (c) mode resolution matches the runtime contract */
        LvConfig c;
        lv_config_from_state(
            "{\"identity\":{\"movement\":\"platformer_movement\","
            "\"camera\":\"third_person\"}}", &c);
        CHECK(c.mode == LV_MODE_2D5,
              "world: platformer_movement + third_person -> Side2D5");
        lv_config_from_state(
            "{\"identity\":{\"movement\":\"free_roam_movement\","
            "\"camera\":\"third_person\"}}", &c);
        CHECK(c.mode == LV_MODE_3D,
              "world: free_roam_movement + third_person -> Orbit3D");
    }

    printf("\n%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
