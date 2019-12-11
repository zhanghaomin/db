#ifndef _DB_H
#define _DB_H
#include <string.h>

typedef enum {
    INTEGER,
    DOUBLE,
    STRING
} DBValType;

typedef struct {
    DBValType type;
    union {
        int num;
        double d;
        char* str;
    } val;
} DBVal;

typedef enum {
    C_INT,
    C_DOUBLE,
    C_CHAR,
    C_VARCHAR
} ColType;

typedef enum {
    EQ, // =
    GT, // >
    LT, // <
    GTE, // >=
    LTE, // <=
    NEQ, // != <>
    W_AND, // and
    W_OR // or
} WhereOP;

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