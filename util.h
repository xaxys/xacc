#ifndef UTIL_H
#define UTIL_H

typedef struct Vector {
    void **data;
    int capacity;
    int len;
} Vector;

Vector *NewVector();
void VectorPush(Vector *v, void *elem);
void VectorPushInt(Vector *v, int val);
void *VectorPop(Vector *v);
void *VectorGet(Vector *v, int i);
int VectorGetInt(Vector *v, int i);
void *VectorLast(Vector *v);
int VectorContain(Vector *v, void *elem);
int VectorUnion(Vector *v, void *elem);
int VectorSize(Vector *v);

typedef struct Map {
    Vector *keys;
    Vector *vals;
} Map;

Map *NewMap();
void MapPut(Map *map, char *key, void *val);
void MapPutInt(Map *map, char *key, int val);
void *MapGet(Map *map, char *key);
int MapGetInt(Map *map, char *key, int _default);

typedef struct {
    char *data;
    int   capacity;
    int   len;
} StringBuilder;

StringBuilder *NewStringBuilder();
void StringBuilderAdd(StringBuilder *sb, char c);
void StringBuilderAppend(StringBuilder *sb, char *s);
void StringBuilderAppendN(StringBuilder *sb, char *s, int len);
char *StringBuilderToString(StringBuilder *sb);

char *Format(char *fmt, ...);

#endif