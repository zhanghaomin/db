#ifndef _BTREE_H
#define _BTREE_H
#include "pager.h"

typedef struct {
    char is_leaf;
    int parent;
    int rec_num;
} btree_node;

typedef struct {
    int root;
    int last_page;
} btree;

typedef struct {
    int id;
    char name[32];
    char address[255];
} db_row;

struct _db_table {
    int row_cnt;
    btree* primary_data;
    table_pager* pager;
};

typedef struct _db_table db_table;

int get_row_size();
int btree_find(db_table* t, int id);
void btree_insert(db_table* t, db_row* r);
#endif
