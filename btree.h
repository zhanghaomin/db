#ifndef _BTREE_H
#define _BTREE_H
#include "table.h"

typedef struct {
    int id;
    int row_pos;
} page_directory_entry;

typedef struct {
    char is_leaf;
    int parent;
    int rec_num;
    int free;
    int row_tail;
    page_directory_entry page_directory[];
} btree_node;

typedef struct {
    int root;
} btree;
#endif
