#ifndef _AST_H
#define _AST_H

#include "db_val.h"

typedef enum {
    AST_VAL,
    AST_SELECT,
    AST_EXPECT_COLS,
    AST_WHERE_NODE,
    AST_WHERE_LEAF
} ast_kind;

typedef struct _ast {
    ast_kind kind;
    int op;
    int children;
    struct _ast** child;
} ast;

typedef struct {
    ast_kind kind;
    db_val* val;
} ast_val;

typedef enum {
    EQ, // =
    GT, // >
    LT, // <
    GTE, // >=
    LTE, // <=
    NEQ // != <>
} cmp_op;

typedef enum {
    W_AND, // and
    W_OR // or
} logic_op;

#define create_val_ast(_t, _v) ({         \
    ast_val* _va;                         \
    _va = smalloc(sizeof(ast_val));       \
    _va->kind = AST_VAL;                  \
    _va->val = smalloc(sizeof(db_val));   \
    _va->val = DB_VAl_##_t(_va->val, _v); \
    (ast*)_va;                            \
})

ast* create_ast(int children, ast_kind kind, int op, ...);
void ast_add_child(ast* a, ast* child);

ast* G_AST;

#endif