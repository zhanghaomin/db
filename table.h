#ifndef _TABLE_H
#define _TABLE_H

#include "btree.h"
#include "pager.h"

// id int
// name char(32)
// address char(255)
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

#endif