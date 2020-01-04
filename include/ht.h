#ifndef HT_H
#define HT_H 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ht_insert(ht, key, val) _Generic(key,             \
                                         int              \
                                         : ht_insert_int, \
                                         char*            \
                                         : ht_insert_str, \
                                         default          \
                                         : printf("not support type.\n"))(ht, key, val)

#define ht_find(ht, key) _Generic(key,           \
                                  int            \
                                  : ht_find_int, \
                                  char*          \
                                  : ht_find_str, \
                                  default        \
                                  : printf("not support type.\n"))(ht, key)

typedef struct _Bucket {
    struct _Bucket* next;
    void* key;
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
void ht_insert_int(HashTable* ht, int key, void* val);
void ht_insert_str(HashTable* ht, char* key, void* val);
void ht_resize(HashTable* ht);
void* ht_find_int(HashTable* ht, int key);
void* ht_find_str(HashTable* ht, char* key);
unsigned int ht_hash(char* key);
void ht_release(HashTable* ht);
int ht_delete(HashTable* ht, char* key);

#define FOREACH_HT(ht, _k, _v)               \
    do {                                     \
        Bucket* _c;                          \
        char* _k UNUSED;                     \
        void* _v UNUSED;                     \
        for (int i = 0; i < ht->size; ++i) { \
            _c = ht->data[i];                \
            while (_c != NULL) {             \
                _k = _c->key;                \
                _v = _c->val;

#define ENDFOREACH() \
    _c = _c->next;   \
    }                \
    }                \
    }                \
    while (0)        \
        ;
#endif