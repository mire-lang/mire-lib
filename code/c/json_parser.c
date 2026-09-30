// JSON parser and serializer for Mire - written in C for performance
// and to avoid Mire's MSS ownership issues with recursive descent parsing.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// Forward declarations
typedef struct JsonNode JsonNode;

struct JsonNode {
    int type; // 0=null, 1=bool, 2=number, 3=string, 4=array, 5=object
    int bool_val;
    double num_val;
    char *str_val;
    JsonNode **items;
    char **keys;
    JsonNode **vals;
    int len;
    int cap;
};

static JsonNode *json_new(int type) {
    JsonNode *n = (JsonNode *)calloc(1, sizeof(JsonNode));
    n->type = type;
    return n;
}

static void json_free(JsonNode *n) {
    if (!n) return;
    if (n->type == 3) free(n->str_val);
    if (n->type == 4) {
        for (int i = 0; i < n->len; i++) json_free(n->items[i]);
        free(n->items);
    }
    if (n->type == 5) {
        for (int i = 0; i < n->len; i++) {
            free(n->keys[i]);
            json_free(n->vals[i]);
        }
        free(n->keys);
        free(n->vals);
    }
    free(n);
}

// Parser state
typedef struct {
    const char *input;
    int pos;
    int len;
} PS;

static char ps_peek(PS *s) {
    if (s->pos >= s->len) return 0;
    return s->input[s->pos];
}

static char ps_advance(PS *s) {
    if (s->pos >= s->len) return 0;
    return s->input[s->pos++];
}

static void ps_skip_ws(PS *s) {
    while (s->pos < s->len) {
        char c = s->input[s->pos];
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') { s->pos++; }
        else break;
    }
}

// Escape a string for JSON output
static char *json_escape_string(const char *s) {
    int slen = (int)strlen(s);
    char *buf = (char *)malloc(slen * 6 + 3);
    int j = 0;
    buf[j++] = '"';
    for (int i = 0; i < slen; i++) {
        char c = s[i];
        if (c == '"') { buf[j++] = '\\'; buf[j++] = '"'; }
        else if (c == '\\') { buf[j++] = '\\'; buf[j++] = '\\'; }
        else if (c == '\n') { buf[j++] = '\\'; buf[j++] = 'n'; }
        else if (c == '\r') { buf[j++] = '\\'; buf[j++] = 'r'; }
        else if (c == '\t') { buf[j++] = '\\'; buf[j++] = 't'; }
        else { buf[j++] = c; }
    }
    buf[j++] = '"';
    buf[j] = '\0';
    return buf;
}

// Serialize a JSON node to string
char *json_stringify(JsonNode *n) {
    if (!n) {
        char *r = strdup("null");
        return r;
    }
    switch (n->type) {
        case 0: return strdup("null");
        case 1: return strdup(n->bool_val ? "true" : "false");
        case 2: {
            char buf[64];
            if (n->num_val == (int)n->num_val && fabs(n->num_val) < 1e15)
                snprintf(buf, sizeof(buf), "%d", (int)n->num_val);
            else
                snprintf(buf, sizeof(buf), "%g", n->num_val);
            return strdup(buf);
        }
        case 3: return json_escape_string(n->str_val);
        case 4: {
            int cap = 256;
            char *r = (char *)malloc(cap);
            int j = 0;
            r[j++] = '[';
            for (int i = 0; i < n->len; i++) {
                if (i > 0) r[j++] = ',';
                char *inner = json_stringify(n->items[i]);
                int ilen = (int)strlen(inner);
                while (j + ilen + 2 > cap) { cap *= 2; r = realloc(r, cap); }
                memcpy(r + j, inner, ilen);
                j += ilen;
                free(inner);
            }
            r[j++] = ']';
            r[j] = '\0';
            return r;
        }
        case 5: {
            int cap = 256;
            char *r = (char *)malloc(cap);
            int j = 0;
            r[j++] = '{';
            for (int i = 0; i < n->len; i++) {
                if (i > 0) r[j++] = ',';
                char *ek = json_escape_string(n->keys[i]);
                int klen = (int)strlen(ek);
                while (j + klen + 10 > cap) { cap *= 2; r = realloc(r, cap); }
                memcpy(r + j, ek, klen);
                j += klen;
                free(ek);
                r[j++] = ':';
                char *ev = json_stringify(n->vals[i]);
                int vlen = (int)strlen(ev);
                while (j + vlen + 2 > cap) { cap *= 2; r = realloc(r, cap); }
                memcpy(r + j, ev, vlen);
                j += vlen;
                free(ev);
            }
            r[j++] = '}';
            r[j] = '\0';
            return r;
        }
    }
    return strdup("null");
}

// Append to array
static void arr_append(JsonNode *arr, JsonNode *val) {
    if (arr->len >= arr->cap) {
        arr->cap = arr->cap ? arr->cap * 2 : 8;
        arr->items = realloc(arr->items, sizeof(JsonNode*) * arr->cap);
    }
    arr->items[arr->len++] = val;
}

// Append to object
static void obj_append(JsonNode *obj, const char *key, JsonNode *val) {
    if (obj->len >= obj->cap) {
        obj->cap = obj->cap ? obj->cap * 2 : 8;
        obj->keys = realloc(obj->keys, sizeof(char*) * obj->cap);
        obj->vals = realloc(obj->vals, sizeof(JsonNode*) * obj->cap);
    }
    obj->keys[obj->len] = strdup(key);
    obj->vals[obj->len] = val;
    obj->len++;
}

// Parse a JSON string (unescaped)
static char *ps_parse_str_raw(PS *s) {
    ps_advance(s); // skip opening "
    int cap = 64;
    char *buf = (char *)malloc(cap);
    int j = 0;
    while (s->pos < s->len) {
        char c = ps_advance(s);
        if (c == '"') break;
        if (c == '\\') {
            char esc = ps_advance(s);
            if (esc == '"') buf[j++] = '"';
            else if (esc == '\\') buf[j++] = '\\';
            else if (esc == 'n') buf[j++] = '\n';
            else if (esc == 'r') buf[j++] = '\r';
            else if (esc == 't') buf[j++] = '\t';
            else if (esc == 'b') buf[j++] = '\b';
            else if (esc == 'f') buf[j++] = '\f';
            else if (esc == 'u') {
                // Simple \uXXXX: just pass through as-is for now
                buf[j++] = '\\';
                buf[j++] = 'u';
                for (int k = 0; k < 4 && s->pos < s->len; k++)
                    buf[j++] = ps_advance(s);
            }
            else buf[j++] = esc;
        } else {
            buf[j++] = c;
        }
        if (j >= cap - 2) { cap *= 2; buf = realloc(buf, cap); }
    }
    buf[j] = '\0';
    return buf;
}

// Parse a number
static double ps_parse_number(PS *s) {
    int start = s->pos;
    if (ps_peek(s) == '-') ps_advance(s);
    while (s->pos < s->len && isdigit(ps_peek(s))) ps_advance(s);
    if (s->pos < s->len && ps_peek(s) == '.') {
        ps_advance(s);
        while (s->pos < s->len && isdigit(ps_peek(s))) ps_advance(s);
    }
    if (s->pos < s->len && (ps_peek(s) == 'e' || ps_peek(s) == 'E')) {
        ps_advance(s);
        if (s->pos < s->len && (ps_peek(s) == '+' || ps_peek(s) == '-'))
            ps_advance(s);
        while (s->pos < s->len && isdigit(ps_peek(s))) ps_advance(s);
    }
    int end = s->pos;
    char *num_str = (char *)malloc(end - start + 1);
    memcpy(num_str, s->input + start, end - start);
    num_str[end - start] = '\0';
    double val = strtod(num_str, NULL);
    free(num_str);
    return val;
}

// Parse a JSON value
static JsonNode *ps_parse_value(PS *s) {
    ps_skip_ws(s);
    if (s->pos >= s->len) return json_new(0);
    char c = ps_peek(s);
    if (c == '{') {
        ps_advance(s);
        JsonNode *obj = json_new(5);
        ps_skip_ws(s);
        if (ps_peek(s) == '}') { ps_advance(s); return obj; }
        while (1) {
            ps_skip_ws(s);
            char *key = ps_parse_str_raw(s);
            ps_skip_ws(s);
            ps_advance(s); // skip ':'
            JsonNode *val = ps_parse_value(s);
            obj_append(obj, key, val);
            free(key);
            ps_skip_ws(s);
            c = ps_peek(s);
            if (c == '}') { ps_advance(s); break; }
            if (c == ',') ps_advance(s);
        }
        return obj;
    }
    if (c == '[') {
        ps_advance(s);
        JsonNode *arr = json_new(4);
        ps_skip_ws(s);
        if (ps_peek(s) == ']') { ps_advance(s); return arr; }
        while (1) {
            ps_skip_ws(s);
            JsonNode *val = ps_parse_value(s);
            arr_append(arr, val);
            ps_skip_ws(s);
            c = ps_peek(s);
            if (c == ']') { ps_advance(s); break; }
            if (c == ',') ps_advance(s);
        }
        return arr;
    }
    if (c == '"') {
        char *s_val = ps_parse_str_raw(s);
        JsonNode *n = json_new(3);
        n->str_val = s_val;
        return n;
    }
    if (c == 't') {
        for (int i = 0; i < 4; i++) ps_advance(s);
        JsonNode *n = json_new(1);
        n->bool_val = 1;
        return n;
    }
    if (c == 'f') {
        for (int i = 0; i < 5; i++) ps_advance(s);
        JsonNode *n = json_new(1);
        n->bool_val = 0;
        return n;
    }
    if (c == 'n') {
        for (int i = 0; i < 4; i++) ps_advance(s);
        return json_new(0);
    }
    if (c == '-' || isdigit(c)) {
        JsonNode *n = json_new(2);
        n->num_val = ps_parse_number(s);
        return n;
    }
    return json_new(0);
}

// Public API: parse JSON string, returns opaque pointer
void *c_json_parse(const char *input) {
    PS s = { input, 0, (int)strlen(input) };
    return ps_parse_value(&s);
}

// Public API: free a parsed JSON tree
void c_json_free(void *node) {
    json_free((JsonNode *)node);
}

// Public API: stringify a parsed JSON tree to a malloc'd string
char *c_json_stringify(void *node) {
    return json_stringify((JsonNode *)node);
}

// Public API: get type tag (0-5)
int c_json_type(void *node) {
    if (!node) return 0;
    return ((JsonNode *)node)->type;
}

// Public API: get string value (type 3)
const char *c_json_str_val(void *node) {
    if (!node) return "";
    JsonNode *n = (JsonNode *)node;
    return n->str_val ? n->str_val : "";
}

// Public API: get number value (type 2)
double c_json_num_val(void *node) {
    if (!node) return 0.0;
    return ((JsonNode *)node)->num_val;
}

// Public API: get number value as i64 (type 2)
long long c_json_num_val_i64(void *node) {
    if (!node) return 0;
    return (long long)((JsonNode *)node)->num_val;
}

// Public API: get bool value (type 1)
int c_json_bool_val(void *node) {
    if (!node) return 0;
    return ((JsonNode *)node)->bool_val;
}

// Public API: get array length (type 4)
int c_json_arr_len(void *node) {
    if (!node) return 0;
    JsonNode *n = (JsonNode *)node;
    return n->type == 4 ? n->len : 0;
}

// Public API: get array item (type 4)
void *c_json_arr_get(void *node, int index) {
    if (!node) return NULL;
    JsonNode *n = (JsonNode *)node;
    if (n->type != 4 || index < 0 || index >= n->len) return NULL;
    return n->items[index];
}

// Public API: get object length (type 5)
int c_json_obj_len(void *node) {
    if (!node) return 0;
    JsonNode *n = (JsonNode *)node;
    return n->type == 5 ? n->len : 0;
}

// Public API: get object key at index
const char *c_json_obj_key(void *node, int index) {
    if (!node) return "";
    JsonNode *n = (JsonNode *)node;
    if (n->type != 5 || index < 0 || index >= n->len) return "";
    return n->keys[index];
}

// Public API: get object value at index
void *c_json_obj_val(void *node, int index) {
    if (!node) return NULL;
    JsonNode *n = (JsonNode *)node;
    if (n->type != 5 || index < 0 || index >= n->len) return NULL;
    return n->vals[index];
}

// Public API: get object value by key
void *c_json_obj_get(void *node, const char *key) {
    if (!node) return NULL;
    JsonNode *n = (JsonNode *)node;
    if (n->type != 5) return NULL;
    for (int i = 0; i < n->len; i++) {
        if (strcmp(n->keys[i], key) == 0) return n->vals[i];
    }
    return NULL;
}

// Public API: create null
void *c_json_new_null(void) { return json_new(0); }

// Public API: create bool
void *c_json_new_bool(int b) {
    JsonNode *n = json_new(1);
    n->bool_val = b;
    return n;
}

// Public API: create number
void *c_json_new_num(double v) {
    JsonNode *n = json_new(2);
    n->num_val = v;
    return n;
}

// Public API: create string
void *c_json_new_str(const char *s) {
    JsonNode *n = json_new(3);
    n->str_val = strdup(s);
    return n;
}

// Public API: create empty array
void *c_json_new_arr(void) { return json_new(4); }

// Public API: append to array
void c_json_arr_push(void *arr, void *val) {
    if (!arr) return;
    arr_append((JsonNode *)arr, val);
}

// Public API: create empty object
void *c_json_new_obj(void) { return json_new(5); }

// Public API: set object key-value
void c_json_obj_set(void *obj, const char *key, void *val) {
    if (!obj) return;
    obj_append((JsonNode *)obj, key, val);
}
