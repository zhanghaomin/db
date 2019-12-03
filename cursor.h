#ifndef _CURSOR_H
#define _CURSOR_H

#include "table.h"

struct _cursor {
    struct _db_table* table;
    int row_num;
    int end;
};

typedef struct _cursor cursor;
#endif