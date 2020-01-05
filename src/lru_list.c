#include "include/lru_list.h"
#include "include/util.h"

LruList* lru_list_init(int size, HtValueCtor ctor, HtValueDtor dtor)
{
    LruList* l;
    l = smalloc(sizeof(LruList));
    l->cap = size;
    l->len = 0;
    l->elements = ht_init(NULL, NULL);
    l->head = l->tail = NULL;
    l->val_ctor = ctor;
    l->val_dtor = dtor;
    return l;
}

static void evict_node_if_needed(LruList* l)
{
    LruListNode* ln;

    if (l->len <= l->cap) {
        return;
    }

    // 去掉最后一个
    ln = l->tail;

    if (ln->prev == NULL) {
        l->head = NULL;
    } else {
        ln->prev->next = ln->next;
    }

    l->tail = ln->prev;
    l->len--;
    ht_delete(l->elements, ln->key);

    if (l->val_ctor != NULL) {
        l->val_dtor(ln->data);
    }

    free(ln->key);
    free(ln);
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
    } else {
        ln->data = data;
    }

    // 放到第一个
    ln->next = l->head;
    ln->prev = NULL;
    ln->key = strdup(key);

    if (l->head == NULL) {
        l->tail = ln;
    } else {
        l->head->prev = ln;
    }

    l->head = ln;
    l->len++;
    ht_insert(l->elements, key, ln);
    evict_node_if_needed(l);
    return ln;
}

void* lru_list_get(LruList* l, char* key)
{
    LruListNode* ln;

    if ((ln = ht_find(l->elements, key)) == NULL) {
        return NULL;
    }

    // 放到第一个
    if (ln->prev == NULL) {
        return ln->data;
    }

    ln->prev->next = ln->next;

    if (ln->next == NULL) {
        l->tail = ln->prev;
    } else {
        ln->next->prev = ln->prev;
    }

    ln->next = l->head;
    l->head = ln;
    ln->prev = NULL;
    return ln->data;
}