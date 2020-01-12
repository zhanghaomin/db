#ifndef _BTREE_H
#define _BTREE_H

typedef enum {
    BT_INT,
    BT_STR
} BtreeKeyType;

typedef struct _Btree {
    BtreeKeyType key_type;
    int height;
    int root;
    int fd;
} Btree;

Btree* btree_init(BtreeKeyType key_type, int fd);
void btree_destory(Btree* bt);
void* btree_insert(Btree* bt, void* key, int len, int page_num, int dir_num);
int btree_find(Btree* bt, void* key, int key_len, int* page_num, int* dir_num);
#endif