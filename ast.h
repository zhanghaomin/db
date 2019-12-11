#ifndef _AST_H
#define _AST_H

#include "db.h"

#define CHILD_CNT_OFFSET 10
#define MAX_CHILD_CNT 1 << 15
#define child_is_unlimited(_kind) ((_kind) >> CHILD_CNT_OFFSET == MAX_CHILD_CNT)
#define max_child_cnt(_kind) ((_kind) >> CHILD_CNT_OFFSET)
#define get_kind_offset(_kind) ((_kind) - (max_child_cnt(_kind) << CHILD_CNT_OFFSET))
#define get_kind_name(_kind) ({                                                   \
    const char* _name;                                                            \
    if (child_is_unlimited(_kind)) {                                              \
        _name = UnlimitedKindNameMap[get_kind_offset(_kind)];                     \
    } else {                                                                      \
        _name = LimitedKindNameMap[max_child_cnt(_kind)][get_kind_offset(_kind)]; \
    }                                                                             \
    _name;                                                                        \
})

static const char* LimitedKindNameMap[][1024] = {
    { "AST_VAL" }, // 0 child
    { "AST_TABLE", "AST_ORDER_BY_COL" }, // 1 child
    { "AST_WHERE_NODE", "AST_WHERE_LEAF", "AST_COL_FMT", "AST_CREATE", "AST_INSERT", "AST_LIMIT" }, // 2 child
    {}, // 3 child
    {}, // 4 child
    { "AST_SELECT" }, // 5 child
};

static const char* UnlimitedKindNameMap[] = { "AST_EXPECT_COLS", "AST_COL_FMT_LIST", "AST_INSERT_ROW_LIST", "AST_INSERT_VAL_LIST", "AST_ORDER_BY" };

typedef enum {
    // 0 child
    AST_VAL = 0 << CHILD_CNT_OFFSET,
    // 1 child
    AST_TABLE = 1 << CHILD_CNT_OFFSET,
    AST_ORDER_BY_COL,
    // 2 child
    AST_WHERE_NODE = 2 << CHILD_CNT_OFFSET,
    AST_WHERE_LEAF,
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

Ast* create_ast(int children, AstKind kind, int attr, ...);
Ast* ast_add_child(Ast* a, Ast* child);
void print_ast(Ast* a, int step, int last);

Ast* G_AST;

#endif