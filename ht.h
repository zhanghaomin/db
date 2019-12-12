#ifndef HT_H
#define HT_H 1

#include <stdlib.h>
#include <string.h>

typedef struct _Bucket {
    struct _Bucket* next;
    char* key;
    void* val;
} Bucket;

typedef void* (*HtValueCtor)(void*);
typedef void (*HtValueDtor)(void*);

typedef struct {
    int cnt;
    int size;
    HtValueCtor val_ctor;
    HtValueDtor val_dtor;
    Bucket** data;
} HashTable;

HashTable* ht_init(HtValueCtor ctor, HtValueDtor dtor);
void ht_insert(HashTable* ht, char* key, void* val);
void ht_resize(HashTable* ht);
void* ht_find(HashTable* ht, char* key);
unsigned int ht_hash(char* key);
int ht_delete(HashTable* ht, char* key);

#endif