/* litt_json.h - minimal dependency-free JSON scanner for Litt (C11).
 * Port of src/gameplay.rs Json. Owns its tree; free with lvj_free. */
#ifndef LITT_JSON_H
#define LITT_JSON_H

enum {
    LJ_NULL = 0, LJ_BOOL, LJ_NUM, LJ_STR, LJ_ARR, LJ_OBJ
};

typedef struct LvJson LvJson;
struct LvJson {
    int kind;
    double num;        /* LJ_NUM */
    int boolean;       /* LJ_BOOL */
    char *str;         /* LJ_STR */
    LvJson **items;    /* LJ_ARR / LJ_OBJ values */
    char **keys;       /* LJ_OBJ keys */
    int count;
};

LvJson *lvj_parse(const char *text);
const LvJson *lvj_get(const LvJson *v, const char *key);
const LvJson *lvj_at(const LvJson *v, int i);
double lvj_num(const LvJson *v, double def);
int lvj_bool(const LvJson *v, int def);
const char *lvj_str(const LvJson *v, const char *def);
int lvj_arr_f3(const LvJson *v, float out[3]);
void lvj_free(LvJson *v);

#endif
