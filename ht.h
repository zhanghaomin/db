#ifndef HT_H
#define HT_H 1

#include <stdlib.h>
#include <string.h>

typedef struct bucket_s {
	struct bucket_s *next;
	char *key;
	char *val;
} bucket;

typedef void *(*ht_value_ctor)(void *);
typedef void (*ht_value_dtor)(void *);

typedef struct ht_s {
	unsigned int cnt;
	unsigned int size;
	ht_value_ctor val_ctor;
	ht_value_dtor val_dtor;
	bucket **data;
} hashtable;

hashtable *ht_init();
void ht_insert(hashtable *ht, char *key, void *val);
void ht_resize(hashtable *ht);
void *ht_find(hashtable *ht, char *key);
unsigned int ht_hash(char *key);
int ht_delete(hashtable *ht, char *key);

hashtable *ht_init(ht_value_ctor ctor, ht_value_dtor dtor)
{
	hashtable *ht;
	ht = malloc(sizeof(hashtable));
	ht->cnt = 0;
	ht->size = 8;
	ht->data = malloc(8 * sizeof(bucket *));
	ht->val_ctor = ctor;
	ht->val_dtor = dtor;
	memset(ht->data, 0, 8 * sizeof(bucket *));
	return ht;
}

void ht_insert(hashtable *ht, char *key, void *val)
{
	bucket *b;
	bucket *tmp;
	bucket *newbucket;
	unsigned int pos;
	int find = 0; 
	pos = ht_hash(key) % ht->size;
	b = ht->data[pos];
	tmp = b;

	while (tmp != NULL) {
		if (strcmp(key, tmp->key) == 0) {
			ht->val_dtor(tmp->val);
			tmp->val = ht->val_ctor(val);
			return;
		}

		tmp = tmp->next;
	}

	newbucket = malloc(sizeof(bucket));
	newbucket->key = strdup(key);
	newbucket->val = ht->val_ctor(val);
	newbucket->next = b;
	ht->data[pos] = newbucket;

	if ((double)(++ht->cnt) / (double)(ht->size) > 0.75) {
		ht_resize(ht);
	}
}

int ht_delete(hashtable *ht, char *key)
{
	bucket *tmp, *prev;
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
			ht->val_dtor(tmp->val);
			free(tmp);
			ht->cnt--;
			return 1;
		}

		prev = tmp;
		tmp = tmp->next;
	}

	return 0;
}

void ht_resize(hashtable *ht)
{
	bucket **newdata;
	bucket *current;
	bucket *tmp;
	unsigned int newsize;
	newsize = ht->size * 2;
	newdata = malloc(newsize * sizeof(bucket *));
	memset(newdata, 0, newsize * sizeof(bucket *));

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

void *ht_find(hashtable *ht, char *key)
{
	bucket *tmp;
	tmp = ht->data[ht_hash(key) % ht->size];

	while (tmp != NULL) {
		if (strcmp(key, tmp->key) == 0) {
			return tmp->val;
		}

		tmp = tmp->next;
	}

	return NULL;
}

void ht_release(hashtable *ht)
{
	bucket *current;
	bucket *tmp;

	for (int i = 0; i < ht->size; ++i) {
		current = ht->data[i];

		while (current != NULL) {
			tmp = current->next;
			free(current->key);
			ht->val_dtor(current->val);
			free(current);
			current = tmp;
		}
	}

	free(ht->data);
	free(ht);
}

unsigned int ht_hash(char *key)
{
	unsigned int hash = 0;

	for (; *key != '\0'; key++)
		hash += hash * 31 + *key;

	return hash;
}

#endif