#include "include/lru_list.h"
#include "include/util.h"

LruList* lru_list_init(int size, HtValueCtor ctor, HtValueDtor dtor)
{
    LruList* l;
    l = smalloc(sizeof(LruList));
    l->cap = size;
    l->elements = ht_init(NULL, NULL);
    l->head = l->tail = NULL;
    l->val_ctor = ctor;
    l->val_dtor = dtor;
    return l;
}

void* lru_list_add(LruList* l, char* key, void* data)
{
    LruListNode* ln;
    if (ht_find(l->elements, key) != NULL) {
        return NULL;
    }

    ln = smalloc(sizeof(LruListNode));

    if (l->val_ctor != NULL) {
        ln->data = l->val_ctor(data);
    }

    ln->data =
}