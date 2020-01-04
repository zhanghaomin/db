#include "include/ht.h"

HashTable* ht_init(HtValueCtor ctor, HtValueDtor dtor)
{
    HashTable* ht;
    ht = malloc(sizeof(HashTable));
    ht->cnt = 0;
    ht->size = 8;
    ht->data = malloc(8 * sizeof(Bucket*));
    ht->val_ctor = ctor;
    ht->val_dtor = dtor;
    memset(ht->data, 0, 8 * sizeof(Bucket*));
    return ht;
}

void ht_insert_int(HashTable* ht, int key, void* val)
{
    Bucket *b, *tmp, *newbucket;
    unsigned int pos;
    pos = key % ht->size;
    b = ht->data[pos];
    tmp = b;

    while (tmp != NULL) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
        if (key == (int)tmp->key) {
#pragma GCC diagnostic pop
            if (ht->val_dtor != NULL) {
                ht->val_dtor(tmp->val);
            }

            if (ht->val_ctor != NULL) {
                tmp->val = ht->val_ctor(val);
            } else {
                tmp->val = val;
            }

            return;
        }

        tmp = tmp->next;
    }

    newbucket = malloc(sizeof(Bucket));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
    newbucket->key = (void*)key;
#pragma GCC diagnostic pop
    if (ht->val_ctor != NULL) {
        newbucket->val = ht->val_ctor(val);
    } else {
        newbucket->val = val;
    }

    newbucket->next = b;
    ht->data[pos] = newbucket;

    if ((double)(++ht->cnt) / (double)(ht->size) > 0.75) {
        ht_resize(ht);
    }
}

void ht_insert_str(HashTable* ht, char* key, void* val)
{
    Bucket *b, *tmp, *newbucket;
    unsigned int pos;
    pos = ht_hash(key) % ht->size;
    b = ht->data[pos];
    tmp = b;

    while (tmp != NULL) {
        if (strcmp(key, tmp->key) == 0) {
            if (ht->val_dtor != NULL) {
                ht->val_dtor(tmp->val);
            }

            if (ht->val_ctor != NULL) {
                tmp->val = ht->val_ctor(val);
            } else {
                tmp->val = val;
            }

            return;
        }

        tmp = tmp->next;
    }

    newbucket = malloc(sizeof(Bucket));
    newbucket->key = strdup(key);

    if (ht->val_ctor != NULL) {
        newbucket->val = ht->val_ctor(val);
    } else {
        newbucket->val = val;
    }

    newbucket->next = b;
    ht->data[pos] = newbucket;

    if ((double)(++ht->cnt) / (double)(ht->size) > 0.75) {
        ht_resize(ht);
    }
}

int ht_delete(HashTable* ht, char* key)
{
    Bucket *tmp, *prev;
    prev = NULL;
    tmp = ht->data[ht_hash(key) % ht->size];

    while (tmp != NULL) {
        if (strcmp(key, tmp->key) == 0) {
            if (prev) {
                prev->next = tmp->next;
            } else {
                ht->data[ht_hash(key) % ht->size] = tmp->next;
            }

            free(tmp->key);
            if (ht->val_dtor != NULL) {
                ht->val_dtor(tmp->val);
            }
            free(tmp);
            ht->cnt--;
            return 1;
        }

        prev = tmp;
        tmp = tmp->next;
    }

    return 0;
}

void ht_resize(HashTable* ht)
{
    Bucket** newdata;
    Bucket *current, *tmp;
    unsigned int newsize;
    newsize = ht->size * 2;
    newdata = malloc(newsize * sizeof(Bucket*));
    memset(newdata, 0, newsize * sizeof(Bucket*));

    for (int i = 0; i < ht->size; ++i) {
        current = ht->data[i];

        while (current != NULL) {
            tmp = current->next;
            current->next = newdata[ht_hash(current->key) % newsize];
            newdata[ht_hash(current->key) % newsize] = current;
            current = tmp;
        }
    }

    free(ht->data);
    ht->data = newdata;
    ht->size *= 2;
}

void* ht_find_str(HashTable* ht, char* key)
{
    Bucket* tmp;
    tmp = ht->data[ht_hash(key) % ht->size];

    while (tmp != NULL) {
        if (strcmp(key, tmp->key) == 0) {
            return tmp->val;
        }

        tmp = tmp->next;
    }

    return NULL;
}

void* ht_find_int(HashTable* ht, int key)
{
    Bucket* tmp;
    tmp = ht->data[key % ht->size];

    while (tmp != NULL) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
        if (key == (int)tmp->key) {
#pragma GCC diagnostic pop
            return tmp->val;
        }

        tmp = tmp->next;
    }

    return NULL;
}

void ht_release(HashTable* ht)
{
    Bucket *current, *tmp;

    for (int i = 0; i < ht->size; ++i) {
        current = ht->data[i];

        while (current != NULL) {
            tmp = current->next;
            free(current->key);
            if (ht->val_dtor != NULL) {
                ht->val_dtor(current->val);
            }
            free(current);
            current = tmp;
        }
    }

    free(ht->data);
    free(ht);
}

unsigned int ht_hash(char* key)
{
    unsigned int hash = 0;

    for (; *key != '\0'; key++)
        hash += hash * 31 + *key;

    return hash;
}