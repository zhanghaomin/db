#ifndef _BTREE_H
#define _BTREE_H
#include "table.h"

typedef struct {
    char is_leaf;
    int parent;
    int rec_num;
} btree_node;

typedef struct {
    int root;
} btree;
#endif
