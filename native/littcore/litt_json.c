/* litt_json.c - recursive-descent JSON scanner. */
#include "litt_json.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

typedef struct {
    const char *s;
    size_t i, n;
    int depth;
} P;

static LvJson *v_new(int kind) {
    LvJson *v = calloc(1, sizeof(LvJson));
    if (v) v->kind = kind;
    return v;
}

void lvj_free(LvJson *v) {
    if (!v) return;
    free(v->str);
    for (int i = 0; i < v->count; i++) {
        lvj_free(v->items ? v->items[i] : NULL);
        if (v->keys) free(v->keys[i]);
    }
    free(v->items);
    free(v->keys);
    free(v);
}

static void skip_ws(P *p) {
    while (p->i < p->n) {
        char c = p->s[p->i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->i++;
        else break;
    }
}

static char *parse_str_raw(P *p);

static int hex4(P *p, unsigned *out) {
    if (p->i + 4 > p->n) return 0;
    unsigned v = 0;
    for (int k = 0; k < 4; k++) {
        char c = p->s[p->i + k];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return 0;
    }
    p->i += 4;
    *out = v;
    return 1;
}

static void utf8_put(char *b, size_t *len, unsigned cp) {
    if (cp < 0x80) b[(*len)++] = (char)cp;
    else if (cp < 0x800) {
        b[(*len)++] = (char)(0xC0 | (cp >> 6));
        b[(*len)++] = (char)(0x80 | (cp & 0x3F));
    } else {
        b[(*len)++] = (char)(0xE0 | (cp >> 12));
        b[(*len)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        b[(*len)++] = (char)(0x80 | (cp & 0x3F));
    }
}

static char *parse_str_raw(P *p) {
    if (p->i >= p->n || p->s[p->i] != '"') return NULL;
    p->i++;
    size_t cap = 32, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    while (p->i < p->n && p->s[p->i] != '"') {
        char c = p->s[p->i++];
        if (len + 8 > cap) { cap *= 2; buf = realloc(buf, cap); if (!buf) return NULL; }
        if (c == '\\') {
            if (p->i >= p->n) { free(buf); return NULL; }
            char e = p->s[p->i++];
            switch (e) {
            case '"': buf[len++] = '"'; break;
            case '\\': buf[len++] = '\\'; break;
            case '/': buf[len++] = '/'; break;
            case 'b': buf[len++] = '\b'; break;
            case 'f': buf[len++] = '\f'; break;
            case 'n': buf[len++] = '\n'; break;
            case 'r': buf[len++] = '\r'; break;
            case 't': buf[len++] = '\t'; break;
            case 'u': {
                unsigned cp;
                if (!hex4(p, &cp)) { free(buf); return NULL; }
                /* surrogate pair */
                if (cp >= 0xD800 && cp <= 0xDBFF && p->i + 1 < p->n &&
                    p->s[p->i] == '\\' && p->s[p->i + 1] == 'u') {
                    p->i += 2;
                    unsigned lo;
                    if (!hex4(p, &lo)) { free(buf); return NULL; }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                utf8_put(buf, &len, cp);
                break;
            }
            default: free(buf); return NULL;
            }
        } else {
            buf[len++] = c;
        }
    }
    if (p->i >= p->n) { free(buf); return NULL; }
    p->i++; /* closing quote */
    buf[len] = 0;
    return buf;
}

static LvJson *parse_value(P *p);

static int push_item(LvJson *v, char *key, LvJson *item) {
    LvJson **ni = realloc(v->items, sizeof(LvJson *) * (size_t)(v->count + 1));
    if (!ni) return 0;
    v->items = ni;
    v->items[v->count] = item;
    if (v->kind == LJ_OBJ) {
        char **nk = realloc(v->keys, sizeof(char *) * (size_t)(v->count + 1));
        if (!nk) return 0;
        v->keys = nk;
        v->keys[v->count] = key;
    }
    v->count++;
    return 1;
}

static LvJson *parse_value(P *p) {
    if (++p->depth > 128) { p->depth--; return NULL; }
    skip_ws(p);
    if (p->i >= p->n) { p->depth--; return NULL; }
    char c = p->s[p->i];
    LvJson *v = NULL;
    if (c == '{') {
        p->i++;
        v = v_new(LJ_OBJ);
        skip_ws(p);
        if (p->i < p->n && p->s[p->i] == '}') { p->i++; p->depth--; return v; }
        for (;;) {
            skip_ws(p);
            char *key = parse_str_raw(p);
            if (!key) { lvj_free(v); p->depth--; return NULL; }
            skip_ws(p);
            if (p->i >= p->n || p->s[p->i] != ':') { free(key); lvj_free(v); p->depth--; return NULL; }
            p->i++;
            LvJson *item = parse_value(p);
            if (!item) { free(key); lvj_free(v); p->depth--; return NULL; }
            /* push_item takes ownership of key on success */
            if (!push_item(v, key, item)) {
                free(key); lvj_free(item); lvj_free(v); p->depth--; return NULL;
            }
            skip_ws(p);
            if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
            if (p->i < p->n && p->s[p->i] == '}') { p->i++; p->depth--; return v; }
            lvj_free(v); p->depth--; return NULL;
        }
    } else if (c == '[') {
        p->i++;
        v = v_new(LJ_ARR);
        skip_ws(p);
        if (p->i < p->n && p->s[p->i] == ']') { p->i++; p->depth--; return v; }
        for (;;) {
            LvJson *item = parse_value(p);
            if (!item || !push_item(v, NULL, item)) { lvj_free(item); lvj_free(v); p->depth--; return NULL; }
            skip_ws(p);
            if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
            if (p->i < p->n && p->s[p->i] == ']') { p->i++; p->depth--; return v; }
            lvj_free(v); p->depth--; return NULL;
        }
    } else if (c == '"') {
        char *s = parse_str_raw(p);
        if (!s) { p->depth--; return NULL; }
        v = v_new(LJ_STR);
        v->str = s;
    } else if (!strncmp(p->s + p->i, "true", (size_t)(p->n - p->i < 4 ? p->n - p->i : 4))) {
        p->i += 4; v = v_new(LJ_BOOL); v->boolean = 1;
    } else if (!strncmp(p->s + p->i, "false", (size_t)(p->n - p->i < 5 ? p->n - p->i : 5))) {
        p->i += 5; v = v_new(LJ_BOOL);
    } else if (!strncmp(p->s + p->i, "null", (size_t)(p->n - p->i < 4 ? p->n - p->i : 4))) {
        p->i += 4; v = v_new(LJ_NULL);
    } else {
        /* number */
        size_t start = p->i;
        if (p->i < p->n && (p->s[p->i] == '-' || p->s[p->i] == '+')) p->i++;
        while (p->i < p->n && (isdigit((unsigned char)p->s[p->i]) || p->s[p->i] == '.' ||
               p->s[p->i] == 'e' || p->s[p->i] == 'E' || p->s[p->i] == '+' || p->s[p->i] == '-'))
            p->i++;
        if (p->i == start) { p->depth--; return NULL; }
        char tmp[64];
        size_t len = p->i - start;
        if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
        memcpy(tmp, p->s + start, len);
        tmp[len] = 0;
        char *end = NULL;
        double d = strtod(tmp, &end);
        if (!end || *end) { p->depth--; return NULL; }
        v = v_new(LJ_NUM);
        v->num = d;
    }
    p->depth--;
    return v;
}

LvJson *lvj_parse(const char *text) {
    if (!text) return NULL;
    P p = { text, 0, strlen(text), 0 };
    LvJson *v = parse_value(&p);
    return v;
}

const LvJson *lvj_get(const LvJson *v, const char *key) {
    if (!v || v->kind != LJ_OBJ || !key) return NULL;
    for (int i = 0; i < v->count; i++)
        if (v->keys[i] && !strcmp(v->keys[i], key))
            return v->items[i];
    return NULL;
}

const LvJson *lvj_at(const LvJson *v, int i) {
    if (!v || v->kind != LJ_ARR || i < 0 || i >= v->count) return NULL;
    return v->items[i];
}

double lvj_num(const LvJson *v, double def) {
    return (v && v->kind == LJ_NUM) ? v->num : def;
}

int lvj_bool(const LvJson *v, int def) {
    return (v && v->kind == LJ_BOOL) ? v->boolean : def;
}

const char *lvj_str(const LvJson *v, const char *def) {
    return (v && v->kind == LJ_STR) ? v->str : def;
}

int lvj_arr_f3(const LvJson *v, float out[3]) {
    if (!v || v->kind != LJ_ARR || v->count < 3) return 0;
    for (int i = 0; i < 3; i++)
        if (v->items[i]->kind != LJ_NUM) return 0;
    out[0] = (float)v->items[0]->num;
    out[1] = (float)v->items[1]->num;
    out[2] = (float)v->items[2]->num;
    return 1;
}
