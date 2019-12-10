#ifndef _AST_H
#define _AST_H

#include "db_val.h"

#define CHILD_CNT_OFFSET 10
#define MAX_CHILD_CNT 1 << 15
#define child_is_unlimited(_kind) ((_kind) >> CHILD_CNT_OFFSET == MAX_CHILD_CNT)
#define max_child_cnt(_kind) ((_kind) >> CHILD_CNT_OFFSET)

typedef enum {
    // 0 child
    AST_VAL = 0 << CHILD_CNT_OFFSET,
    // 1 child
    AST_TABLE = 1 << CHILD_CNT_OFFSET,
    // 2 child
    AST_WHERE_NODE = 2 << CHILD_CNT_OFFSET,
    AST_WHERE_LEAF,
    AST_COL_FMT,
    AST_CREATE,
    // 3 child
    AST_SELECT = 3 << CHILD_CNT_OFFSET,
    // unlimited
    AST_EXPECT_COLS = MAX_CHILD_CNT << CHILD_CNT_OFFSET,
    AST_COL_FMT_LIST,
} ast_kind;

typedef struct _ast {
    ast_kind kind;
    int attr;
    int children;
    struct _ast** child;
} ast;

typedef struct {
    ast_kind kind;
    db_val* val;
} ast_val;

typedef enum {
    C_INT,
    C_DOUBLE,
    C_CHAR,
    C_VARCHAR
} col_type;

typedef enum {
    EQ, // =
    GT, // >
    LT, // <
    GTE, // >=
    LTE, // <=
    NEQ, // != <>
    W_AND, // and
    W_OR // or
} cmp_op;

#define create_val_ast(_t, _v) ({         \
    ast_val* _va;                         \
    _va = smalloc(sizeof(ast_val));       \
    _va->kind = AST_VAL;                  \
    _va->val = smalloc(sizeof(db_val));   \
    _va->val = DB_VAl_##_t(_va->val, _v); \
    (ast*)_va;                            \
})

ast* create_ast(int children, ast_kind kind, int attr, ...);
void ast_add_child(ast* a, ast* child);
void print_ast(ast* a, int step, int last);

ast* G_AST;

#endif