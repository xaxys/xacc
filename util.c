#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "util.h"
Vector *NewVector() {
    Vector *v = calloc(1, sizeof(Vector));
    v->data = malloc(sizeof(void *) * 16);
    v->capacity = 16;
    v->len = 0;
    return v;
}

void VectorPush(Vector *v, void *elem) {
    if (v->len == v->capacity) {
        if (v->capacity <= 1024) {
            v->capacity *= 2;
        } else {
            v->capacity *= 1.25;
        }
        v->data = realloc(v->data, sizeof(void *) * v->capacity);
    }
    v->data[v->len++] = elem;
}

void VectorPushInt(Vector *v, int val) {
    int *tmp = malloc(sizeof(int));
    *tmp = val;
    VectorPush(v, tmp);
}

void *VectorPop(Vector *v) {
    assert(v->len);
    return v->data[--v->len];
}

void *VectorGet(Vector *v, int i) {
    assert(i < v->len);
    return v->data[i];
}

int VectorGetInt(Vector *v, int i) {
    assert(i < v->len);
    return *((int *)(v->data[i]));
}

void *VectorLast(Vector *v) {
    assert(v->len);
    return v->data[v->len - 1];
}

int VectorContain(Vector *v, void *elem) {
    for (int i = 0; i < v->len; i++)
    if (v->data[i] == elem) return 1;
    else return 0;
}

int VectorUnion(Vector *v, void *elem) {
    if (VectorContain(v, elem)) return 1;
    VectorPush(v, elem);
    return 0;
}

int VectorSize(Vector *v) {
    return v->len;
}

Map *NewMap(void) {
    Map *map = calloc(1, sizeof(Map));
    map->keys = NewVector();
    map->vals = NewVector();
    return map;
}

void MapPut(Map *map, char *key, void *val) {
    VectorPush(map->keys, key);
    VectorPush(map->vals, val);
}

void MapPutInt(Map *map, char *key, int val) {
    VectorPush(map->keys, key);
    VectorPushInt(map->vals, val);
}

void *MapGet(Map *map, char *key) {
    for (int i = map->keys->len - 1; i >= 0; i--) {
        if (!strcmp(map->keys->data[i], key)) {
            return map->vals->data[i];
        }
    }
    return NULL;
}

int MapGetInt(Map *map, char *key, int _default) {
    for (int i = VectorSize(map->keys) - 1; i >= 0; i--) {
        if (!strcmp(VectorGet(map->keys, i), key)) {
            return VectorGetInt(map->vals, i);
        }
    }
    return _default;
}

StringBuilder *NewStringBuilder() {
    StringBuilder *sb = malloc(sizeof(StringBuilder));
    sb->data = malloc(8);
    sb->capacity = 8;
    sb->len = 0;
    return sb;
}

void stringBuilderGrow(StringBuilder *sb, int len) {
    if (sb->len + len <= sb->capacity) {
        return;
    }

    while (sb->len + len > sb->capacity) {
        if (sb->capacity <= 1024) {
            sb->capacity *= 2;
        } else {
            sb->capacity * 1.25;
        }
    }
    sb->data = realloc(sb->data, sb->capacity);
}

void StringBuilderAdd(StringBuilder *sb, char c) {
    stringBuilderGrow(sb, 1);
    sb->data[sb->len++] = c;
}

void StringBuilderAppend(StringBuilder *sb, char *s) {
    StringBuilderAppendN(sb, s, strlen(s));
}

void StringBuilderAppendN(StringBuilder *sb, char *s, int len) {
    stringBuilderGrow(sb, len);
    memcpy(sb->data + sb->len, s, len);
    sb->len += len;
}

char *StringBuilderToString(StringBuilder *sb) {
    StringBuilderAdd(sb, '\0');
    return sb->data;
}

char *Format(char *fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return strdup(buf);
}

char *StringClone(char *s, int len) {
    char *tmp = malloc(len + 1);
    memcpy(tmp, s, len);
    tmp[len + 1] = '\0';
    return tmp;
}