#ifndef _AST_H
#define _AST_H

#include "db_val.h"

typedef enum {
    AST_VAL,
    AST_SELECT,
    AST_EXPECT_COLS,
    AST_TABLE,
    AST_WHERE_NODE,
    AST_WHERE_LEAF,
    AST_COL_FMT,
    AST_COL_FMT_LIST,
    AST_CREATE
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
void print_ast(ast* a, int space_count);

ast* G_AST;

#endif