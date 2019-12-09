#ifndef _DB_VAL_H
#define _DB_VAL_H
#include <string.h>

typedef enum {
    INTEGER,
    DOUBLE,
    STRING
} db_val_type;

typedef struct {
    db_val_type type;
    union {
        int num;
        double d;
        char* str;
    } val;
} db_val;

#define DB_VAl_INTEGER(_v, _i) ({ \
    _v->type = INTEGER;           \
    _v->val.num = _i;             \
    _v;                           \
})

#define DB_VAl_STRING(_v, _s) ({ \
    _v->type = STRING;           \
    _v->val.str = strdup(_s);    \
    _v;                          \
})

#define DB_VAl_DOUBLE(_v, _d) ({ \
    _v->type = FLOAT;            \
    _v->val.str = _d;            \
    _v;                          \
})

#endif