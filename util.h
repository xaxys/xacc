/*
 * @Author: your name
 * @Date: 2020-10-08 11:11:27
 * @LastEditTime: 2020-12-07 09:49:59
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: \libadmind:\C++\xacc\util.h
 */
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
void VectorSet(Vector *v, int i, void *elem); // VectorSet will not free the old element
void VectorReplace(Vector *v, int i, void *elem); // VectorReplace will free the old element
void VectorSetInt(Vector *v, int i, int val);
void *VectorLast(Vector *v);
int VectorContain(Vector *v, void *elem);
int VectorUnion(Vector *v, void *elem);
int VectorSize(Vector *v);

typedef struct Map {
    Vector *keys;
    Vector *vals;
} Map;

Map *NewMap();
void MapPut(Map *map, char *key, void *val); // MapPut will not free the old value
void MapReplace(Map *map, char *key, void *val); // MapReplace will free the old value
void MapPutInt(Map *map, char *key, int val);
int MapIndex(Map *map, char *key);
void *MapGet(Map *map, char *key);
int MapGetInt(Map *map, char *key, int _default);
int MapSize(Map *map);
Vector *MapKeys(Map *map);
Vector *MapVals(Map *map);

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
char *StringClone(char *s, int len);

#endif