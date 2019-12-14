#ifndef _AST_H
#define _AST_H

#include "util.h"

#define CHILD_CNT_OFFSET 10
#define MAX_CHILD_CNT 1 << 15
#define child_is_unlimited(_kind) ((_kind) >> CHILD_CNT_OFFSET == MAX_CHILD_CNT)
#define max_child_cnt(_kind) ((_kind) >> CHILD_CNT_OFFSET)
#define get_kind_offset(_kind) ((_kind) - (max_child_cnt(_kind) << CHILD_CNT_OFFSET))

static const char* LimitedKindNameMap[][1024] UNUSED = {
    { "AST_VAL" }, // 0 child
    { "AST_TABLE", "AST_ORDER_BY_COL", "AST_WHERE_TOP_EXP" }, // 1 child
    { "AST_WHERE_EXP", "AST_COL_FMT", "AST_CREATE", "AST_INSERT", "AST_LIMIT" }, // 2 child
    {}, // 3 child
    {}, // 4 child
    { "AST_SELECT" }, // 5 child
};

static const char* UnlimitedKindNameMap[] UNUSED = { "AST_EXPECT_COLS", "AST_COL_FMT_LIST", "AST_INSERT_ROW_LIST", "AST_INSERT_VAL_LIST", "AST_ORDER_BY" };

#define get_kind_name(_kind) ({                                                   \
    const char* _name;                                                            \
    if (child_is_unlimited(_kind)) {                                              \
        _name = UnlimitedKindNameMap[get_kind_offset(_kind)];                     \
    } else {                                                                      \
        _name = LimitedKindNameMap[max_child_cnt(_kind)][get_kind_offset(_kind)]; \
    }                                                                             \
    _name;                                                                        \
})

typedef enum {
    // 0 child
    AST_VAL = 0 << CHILD_CNT_OFFSET,
    // 1 child
    AST_TABLE = 1 << CHILD_CNT_OFFSET,
    AST_ORDER_BY_COL,
    AST_WHERE_TOP_EXP,
    // 2 child
    AST_WHERE_EXP = 2 << CHILD_CNT_OFFSET,
    AST_COL_FMT,
    AST_CREATE,
    AST_INSERT,
    AST_LIMIT,
    // 5 child
    AST_SELECT = 5 << CHILD_CNT_OFFSET,
    // unlimited
    AST_EXPECT_COLS = MAX_CHILD_CNT << CHILD_CNT_OFFSET,
    AST_COL_FMT_LIST,
    AST_INSERT_ROW_LIST,
    AST_INSERT_VAL_LIST,
    AST_ORDER_BY
} AstKind;

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

// %left T_OR
// %left T_AND
// %left "!" "not"
// %left "=" "<" ">" "<=" ">=" "!=" "<>"
// %left "+" "-" "&" "^" "|"
// %left "*" "/" "%"
// %left "~"
typedef enum {
    E_OR,
    E_AND,
    E_NOT,
    E_EQ,
    E_GT,
    E_LT,
    E_LTE,
    E_GTE,
    E_NEQ,
    E_ADD,
    E_SUB,
    E_B_AND,
    E_B_XOR,
    E_B_OR,
    E_MUL,
    E_DIV,
    E_MOD,
    E_B_NOT
} ExpOp;

typedef enum {
    C_INT,
    C_DOUBLE,
    C_CHAR,
    C_VARCHAR
} ColType;

typedef struct _Ast {
    AstKind kind;
    int attr;
    int children;
    struct _Ast** child;
} Ast;

typedef struct {
    AstKind kind;
    DBVal* val;
} AstVal;

#define create_val_ast(_t, _v) ({         \
    AstVal* _va;                          \
    _va = smalloc(sizeof(AstVal));        \
    _va->kind = AST_VAL;                  \
    _va->val = smalloc(sizeof(DBVal));    \
    _va->val = DB_VAl_##_t(_va->val, _v); \
    _va;                                  \
})

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
    _v->type = DOUBLE;           \
    _v->val.d = _d;              \
    _v;                          \
})

#define AST_VAL_STR(_ast) ({         \
    ((AstVal*)(_ast))->val->val.str; \
})

#define AST_VAL_DOUBLE(_ast) ({    \
    ((AstVal*)(_ast))->val->val.d; \
})

#define AST_VAL_INT(_ast) ({         \
    ((AstVal*)(_ast))->val->val.num; \
})

#define AST_VAL_TYPE(_ast) ({     \
    ((AstVal*)(_ast))->val->type; \
})

#define AST_VAL_LEN(_ast) ({                    \
    int _size;                                  \
    if (AST_VAL_TYPE(_ast) == DOUBLE) {         \
        _size = sizeof(double);                 \
    } else if (AST_VAL_TYPE(_ast) == INTEGER) { \
        _size = sizeof(int);                    \
    } else {                                    \
        _size = strlen(AST_VAL_STR(_ast)) + 1;  \
    }                                           \
    _size;                                      \
})

Ast* create_ast(int children, AstKind kind, int attr, ...);
Ast* ast_add_child(Ast* a, Ast* child);
void print_ast(Ast* a);
void ast_destory(Ast* a);

Ast* G_AST;

#endif