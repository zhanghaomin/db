#ifndef _LRU_LIST_H
#define _LRU_LIST_H

#include "ht.h"

typedef struct _LruListNode {
    struct _LruListNode* next;
    struct _LruListNode* prev;
    char* key;
    void* data;
} LruListNode;

typedef struct {
    LruListNode* head;
    LruListNode* tail;
    HashTable* elements;
    HtValueDtor val_dtor;
    HtValueCtor val_ctor;
    int cap;
    int len;
} LruList;

LruList* lru_list_init(int size, HtValueCtor ctor, HtValueDtor dtor);
void* lru_list_add(LruList* l, char* key, void* data);
void* lru_list_get(LruList* l, char* key);
void* lru_list_head(LruList* l);
void* lru_list_tail(LruList* l);
void** lru_get_all(LruList* l, int* len);
#endif