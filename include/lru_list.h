#ifndef _LRU_LIST_H
#define _LRU_LIST_H

#include "ht.h"
// init(len)
// get(k)
// set(k,v) 可能会导致一个过期
// del(k)
//

typedef struct _LruListNode {
    struct _LruListNode* next;
    struct _LruListNode* prev;
    void* data;
} LruListNode;

typedef struct {
    LruListNode* head;
    LruListNode* tail;
    HashTable* elements;
    HtValueDtor val_dtor;
    HtValueCtor val_ctor;
    int cap;
} LruList;

#endif