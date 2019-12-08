#ifndef _AST_H
#define _AST_H

typedef struct {
    int type;
} ast;

typedef enum {
    INTEGER,
    FLOAT,
    STRING
} ast_val_type;

typedef union {
    ast_val_type type;
    union {
        int num;
        double d;
        char* str;
    } val;
} ast_val;

typedef struct {
    int type;
    char** expect_cols;
    int expect_cols_count;
} select_expect_cols_ast;

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

struct _where_ast {
    int type;
    int is_leaf; // leaf or node
    union {
        cmp_op compare; // leaf
        logic_op logic; // node
    } op;
    union {
        ast* ast; // node
        ast_val* val; // leaf
    } children[2];
};

typedef struct _where_ast where_ast;

typedef struct {
    int type;
    select_expect_cols_ast* expect_cols;
    char* table;
    where_ast* where;
} select_ast;

#endif