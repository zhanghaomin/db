#ifndef _CURSOR_H
#define _CURSOR_H

#include "btree.h"
#include "table.h"

struct _cursor {
    struct _db_table* table;
    btree_node* cnode;
    int row_num;
    int page_num;
    int offset;
    int end;
};

typedef struct _cursor cursor;
#endif