/*
 * toml_parser.c - TOML parser for Mire
 *
 * Simplified TOML parser supporting:
 * - Key-value pairs (strings, integers, floats, booleans)
 * - Tables [table]
 * - Inline tables { key = value }
 * - Arrays [1, 2, 3]
 * - Comments # comment
 * - Basic strings "with escapes"
 * - Literal strings 'no escapes'
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TOML_STRING  0
#define TOML_INT     1
#define TOML_FLOAT   2
#define TOML_BOOL    3
#define TOML_ARRAY   4
#define TOML_TABLE   5
#define TOML_NULL    6

typedef struct TomlNode TomlNode;

struct TomlNode {
    int type;
    char *key;
    union {
        char *str_val;
        long long int_val;
        double float_val;
        int bool_val;
        struct { TomlNode **items; int len; } arr;
        struct { TomlNode **items; int len; } tbl;
    } u;
};

typedef struct {
    const char *src;
    int pos;
    int len;
} TomlParser;

static void skip_ws(TomlParser *p) {
    while (p->pos < p->len) {
        char c = p->src[p->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            p->pos++;
        } else if (c == '#') {
            while (p->pos < p->len && p->src[p->pos] != '\n') p->pos++;
        } else {
            break;
        }
    }
}

static char peek(TomlParser *p) {
    return p->pos < p->len ? p->src[p->pos] : 0;
}

static char next(TomlParser *p) {
    return p->pos < p->len ? p->src[p->pos++] : 0;
}

static int match(TomlParser *p, const char *s) {
    int slen = (int)strlen(s);
    if (p->pos + slen > p->len) return 0;
    if (strncmp(p->src + p->pos, s, slen) != 0) return 0;
    p->pos += slen;
    return 1;
}

static TomlNode *toml_new(int type) {
    TomlNode *n = calloc(1, sizeof(TomlNode));
    n->type = type;
    return n;
}

void toml_free(TomlNode *n) {
    if (!n) return;
    free(n->key);
    switch (n->type) {
        case TOML_STRING: free(n->u.str_val); break;
        case TOML_ARRAY:
            for (int i = 0; i < n->u.arr.len; i++) toml_free(n->u.arr.items[i]);
            free(n->u.arr.items);
            break;
        case TOML_TABLE:
            for (int i = 0; i < n->u.tbl.len; i++) toml_free(n->u.tbl.items[i]);
            free(n->u.tbl.items);
            break;
    }
    free(n);
}

static char *parse_basic_string(TomlParser *p) {
    p->pos++;
    int cap = 256;
    char *buf = malloc(cap);
    int len = 0;
    while (p->pos < p->len) {
        char c = next(p);
        if (c == '"') break;
        if (c == '\\') {
            c = next(p);
            switch (c) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '\\': c = '\\'; break;
                case '"': c = '"'; break;
                case '0': c = '\0'; break;
                default: break;
            }
        }
        if (len + 1 >= cap) { cap *= 2; buf = realloc(buf, cap); }
        buf[len++] = c;
    }
    buf[len] = 0;
    return buf;
}

static char *parse_literal_string(TomlParser *p) {
    p->pos++;
    int start = p->pos;
    while (p->pos < p->len && p->src[p->pos] != '\'') p->pos++;
    int len = p->pos - start;
    if (p->pos < p->len) p->pos++;
    char *buf = malloc(len + 1);
    memcpy(buf, p->src + start, len);
    buf[len] = 0;
    return buf;
}

static char *parse_key(TomlParser *p) {
    skip_ws(p);
    if (peek(p) == '"') return parse_basic_string(p);
    if (peek(p) == '\'') return parse_literal_string(p);
    int start = p->pos;
    while (p->pos < p->len && (isalnum(p->src[p->pos]) || p->src[p->pos] == '-' || p->src[p->pos] == '_'))
        p->pos++;
    int len = p->pos - start;
    char *buf = malloc(len + 1);
    memcpy(buf, p->src + start, len);
    buf[len] = 0;
    return buf;
}

static TomlNode *parse_value(TomlParser *p);

static TomlNode *parse_array(TomlParser *p) {
    p->pos++;
    TomlNode *node = toml_new(TOML_ARRAY);
    int cap = 8;
    node->u.arr.items = malloc(sizeof(TomlNode*) * cap);
    node->u.arr.len = 0;
    skip_ws(p);
    while (peek(p) && peek(p) != ']') {
        TomlNode *item = parse_value(p);
        if (node->u.arr.len >= cap) { cap *= 2; node->u.arr.items = realloc(node->u.arr.items, sizeof(TomlNode*) * cap); }
        node->u.arr.items[node->u.arr.len++] = item;
        skip_ws(p);
        if (peek(p) == ',') { p->pos++; skip_ws(p); }
    }
    if (peek(p) == ']') p->pos++;
    return node;
}

static TomlNode *parse_inline_table(TomlParser *p) {
    p->pos++;
    TomlNode *node = toml_new(TOML_TABLE);
    int cap = 8;
    node->u.tbl.items = malloc(sizeof(TomlNode*) * cap);
    node->u.tbl.len = 0;
    skip_ws(p);
    while (peek(p) && peek(p) != '}') {
        TomlNode *entry = toml_new(TOML_TABLE);
        entry->key = parse_key(p);
        skip_ws(p);
        if (peek(p) == '=') p->pos++;
        skip_ws(p);
        TomlNode *val = parse_value(p);
        val->key = entry->key;
        entry->key = NULL;
        toml_free(entry);
        if (node->u.tbl.len >= cap) { cap *= 2; node->u.tbl.items = realloc(node->u.tbl.items, sizeof(TomlNode*) * cap); }
        node->u.tbl.items[node->u.tbl.len++] = val;
        skip_ws(p);
        if (peek(p) == ',') { p->pos++; skip_ws(p); }
    }
    if (peek(p) == '}') p->pos++;
    return node;
}

static TomlNode *parse_value(TomlParser *p) {
    skip_ws(p);
    char c = peek(p);
    if (c == '"') { TomlNode *n = toml_new(TOML_STRING); n->u.str_val = parse_basic_string(p); return n; }
    if (c == '\'') { TomlNode *n = toml_new(TOML_STRING); n->u.str_val = parse_literal_string(p); return n; }
    if (c == '[') return parse_array(p);
    if (c == '{') return parse_inline_table(p);
    if (c == 't' || c == 'f') {
        TomlNode *n = toml_new(TOML_BOOL);
        if (match(p, "true")) n->u.bool_val = 1;
        else if (match(p, "false")) n->u.bool_val = 0;
        return n;
    }
    /* number */
    {
        int start = p->pos;
        int is_float = 0;
        if (c == '-' || c == '+') p->pos++;
        while (p->pos < p->len && isdigit(p->src[p->pos])) p->pos++;
        if (p->pos < p->len && p->src[p->pos] == '.') { is_float = 1; p->pos++; while (p->pos < p->len && isdigit(p->src[p->pos])) p->pos++; }
        if (p->pos < p->len && (p->src[p->pos] == 'e' || p->src[p->pos] == 'E')) { is_float = 1; p->pos++; if (p->pos < p->len && (p->src[p->pos] == '+' || p->src[p->pos] == '-')) p->pos++; while (p->pos < p->len && isdigit(p->src[p->pos])) p->pos++; }
        int len = p->pos - start;
        char *s = malloc(len + 1);
        memcpy(s, p->src + start, len);
        s[len] = 0;
        if (is_float) {
            TomlNode *n = toml_new(TOML_FLOAT);
            n->u.float_val = strtod(s, NULL);
            free(s);
            return n;
        } else {
            TomlNode *n = toml_new(TOML_INT);
            n->u.int_val = strtoll(s, NULL, 10);
            free(s);
            return n;
        }
    }
}

static void table_set(TomlNode *tbl, char *key, TomlNode *val) {
    val->key = key;
    if (!tbl->u.tbl.items || tbl->u.tbl.len >= 16) {
        int cap = tbl->u.tbl.len ? tbl->u.tbl.len * 2 : 16;
        tbl->u.tbl.items = realloc(tbl->u.tbl.items, sizeof(TomlNode*) * cap);
    }
    tbl->u.tbl.items[tbl->u.tbl.len++] = val;
}

TomlNode *c_toml_parse(const char *input) {
    TomlParser p = {input, 0, (int)strlen(input)};
    TomlNode *root = toml_new(TOML_TABLE);
    int cap = 16;
    root->u.tbl.items = malloc(sizeof(TomlNode*) * cap);
    root->u.tbl.len = 0;

    TomlNode *current = root;

    while (p.pos < p.len) {
        skip_ws(&p);
        if (p.pos >= p.len) break;

        if (peek(&p) == '[') {
            p.pos++;
            int is_array_table = 0;
            if (peek(&p) == '[') { is_array_table = 1; p.pos++; }
            skip_ws(&p);
            char *tbl_key = parse_key(&p);
            skip_ws(&p);
            if (is_array_table && peek(&p) == ']') p.pos++;
            if (peek(&p) == ']') p.pos++;
            skip_ws(&p);

            TomlNode *tbl = toml_new(TOML_TABLE);
            if (is_array_table) {
                TomlNode *arr = toml_new(TOML_ARRAY);
                arr->u.arr.items = malloc(sizeof(TomlNode*) * 4);
                arr->u.arr.len = 1;
                arr->u.arr.items[0] = tbl;
                arr->key = tbl_key;
                if (root->u.tbl.len >= cap) { cap *= 2; root->u.tbl.items = realloc(root->u.tbl.items, sizeof(TomlNode*) * cap); }
                root->u.tbl.items[root->u.tbl.len++] = arr;
            } else {
                tbl->key = tbl_key;
                if (root->u.tbl.len >= cap) { cap *= 2; root->u.tbl.items = realloc(root->u.tbl.items, sizeof(TomlNode*) * cap); }
                root->u.tbl.items[root->u.tbl.len++] = tbl;
            }
            current = tbl;
        } else {
            char *key = parse_key(&p);
            skip_ws(&p);
            if (peek(&p) == '=') p.pos++;
            skip_ws(&p);
            TomlNode *val = parse_value(&p);
            val->key = key;
            if (current->u.tbl.len >= (current->u.tbl.items ? 16 : 0)) {
                int ncap = current->u.tbl.len ? current->u.tbl.len * 2 : 16;
                current->u.tbl.items = realloc(current->u.tbl.items, sizeof(TomlNode*) * ncap);
            }
            if (!current->u.tbl.items) { current->u.tbl.items = malloc(sizeof(TomlNode*) * 16); }
            current->u.tbl.items[current->u.tbl.len++] = val;
        }
    }
    return root;
}

int c_toml_type(void *node) {
    TomlNode *n = (TomlNode *)node;
    return n ? n->type : TOML_NULL;
}

const char *c_toml_str_val(void *node) {
    TomlNode *n = (TomlNode *)node;
    if (!n || n->type != TOML_STRING) return "";
    return n->u.str_val ? n->u.str_val : "";
}

long long c_toml_int_val(void *node) {
    TomlNode *n = (TomlNode *)node;
    if (!n) return 0;
    if (n->type == TOML_INT) return n->u.int_val;
    if (n->type == TOML_FLOAT) return (long long)n->u.float_val;
    return 0;
}

double c_toml_float_val(void *node) {
    TomlNode *n = (TomlNode *)node;
    if (!n) return 0.0;
    if (n->type == TOML_FLOAT) return n->u.float_val;
    if (n->type == TOML_INT) return (double)n->u.int_val;
    return 0.0;
}

int c_toml_bool_val(void *node) {
    TomlNode *n = (TomlNode *)node;
    if (!n || n->type != TOML_BOOL) return 0;
    return n->u.bool_val;
}

int c_toml_arr_len(void *node) {
    TomlNode *n = (TomlNode *)node;
    if (!n || n->type != TOML_ARRAY) return 0;
    return n->u.arr.len;
}

void *c_toml_arr_get(void *node, int index) {
    TomlNode *n = (TomlNode *)node;
    if (!n || n->type != TOML_ARRAY || index < 0 || index >= n->u.arr.len) return NULL;
    return n->u.arr.items[index];
}

int c_toml_tbl_len(void *node) {
    TomlNode *n = (TomlNode *)node;
    if (!n || n->type != TOML_TABLE) return 0;
    return n->u.tbl.len;
}

void *c_toml_tbl_get(void *node, const char *key) {
    TomlNode *n = (TomlNode *)node;
    if (!n || n->type != TOML_TABLE || !key) return NULL;
    for (int i = 0; i < n->u.tbl.len; i++) {
        TomlNode *child = n->u.tbl.items[i];
        if (child->key && strcmp(child->key, key) == 0) return child;
    }
    return NULL;
}

const char *c_toml_tbl_key(void *node, int index) {
    TomlNode *n = (TomlNode *)node;
    if (!n || n->type != TOML_TABLE || index < 0 || index >= n->u.tbl.len) return "";
    TomlNode *child = n->u.tbl.items[index];
    return child->key ? child->key : "";
}
