#include "btree.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char const* argv[])
{
    db_table* t;
    table_pager* p;
    t = (db_table*)calloc(sizeof(db_table), 1);
    p = (table_pager*)calloc(sizeof(table_pager), 1);
    t->pager = p;
    t->primary_data = calloc(sizeof(btree), 1);
    p->fd = open("1.txt", O_RDWR | O_CREAT, 0644);
    printf("%d\n", p->fd);
    db_row r;
    memcpy(r.address, "aaa", 4);
    memcpy(r.address, "bbb", 4);
    r.id = 1;
    btree_insert(t, &r);
    printf("%d\n", btree_find(t, 1));
}
