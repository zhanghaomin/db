#ifndef _AST_H
#define _AST_H

#include "util.h"

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

static const char* LimitedKindNameMap[][1024] UNUSED = {
    { "AST_VAL" }, // 0 child
    { "AST_TABLE", "AST_ORDER_BY_COL", "AST_WHERE_EXP" }, // 1 child
    { "AST_EXP", "AST_COL_FMT", "AST_CREATE", "AST_INSERT", "AST_LIMIT", "AST_DELETE", "AST_UPDATE_SET" }, // 2 child
    {}, // 3 child
    { "AST_UPDATE" }, // 4 child
    { "AST_SELECT" }, // 5 child
};

static const char* UnlimitedKindNameMap[] UNUSED = { "AST_EXPECT_COLS", "AST_COL_FMT_LIST", "AST_INSERT_ROW_LIST", "AST_INSERT_VAL_LIST", "AST_ORDER_BY", "AST_UPDATE_SET_LIST" };

typedef enum {
    // 0 child
    AST_VAL = 0 << CHILD_CNT_OFFSET,
    // 1 child
    AST_TABLE = 1 << CHILD_CNT_OFFSET,
    AST_ORDER_BY_COL,
    AST_WHERE_EXP,
    // 2 child
    AST_EXP = 2 << CHILD_CNT_OFFSET,
    AST_COL_FMT,
    AST_CREATE,
    AST_INSERT,
    AST_LIMIT,
    AST_DELETE,
    AST_UPDATE_SET,
    // 4 child
    AST_UPDATE = 4 << CHILD_CNT_OFFSET,
    // 5 child
    AST_SELECT = 5 << CHILD_CNT_OFFSET,
    // unlimited
    AST_EXPECT_COLS = MAX_CHILD_CNT << CHILD_CNT_OFFSET,
    AST_COL_FMT_LIST,
    AST_INSERT_ROW_LIST,
    AST_INSERT_VAL_LIST,
    AST_ORDER_BY,
    AST_UPDATE_SET_LIST
} AstKind;

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
    E_MUL,
    E_DIV,
    E_MOD,

    E_B_AND,
    E_B_XOR,
    E_B_OR,
    E_B_NOT
} ExprOp;

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

typedef enum {
    AVT_INT,
    AVT_DOUBLE,
    AVT_ID,
    AVT_STR
} AstValType;

typedef struct {
    AstKind kind;
    AstValType type;
    union {
        int num;
        double d;
        char* str;
    } val;
} AstVal;

#define CREATE_AV(_t, _v) ({       \
    AstVal* _va;                   \
    _va = smalloc(sizeof(AstVal)); \
    _va->kind = AST_VAL;           \
    _t##_CTOR(_va, _v);            \
    (Ast*)_va;                     \
})

#define AVT_INT_CTOR(_v, _i) ({ \
    _v->type = AVT_INT;         \
    _v->val.num = _i;           \
    _v;                         \
})

#define AVT_DOUBLE_CTOR(_v, _i) ({ \
    _v->type = AVT_DOUBLE;         \
    _v->val.d = _i;                \
    _v;                            \
})

#define AVT_ID_CTOR(_v, _s) ({ \
    _v->type = AVT_ID;         \
    _v->val.str = strdup(_s);  \
    _v;                        \
})

#define AVT_STR_CTOR(_v, _s) ({ \
    _v->type = AVT_STR;         \
    _v->val.str = strdup(_s);   \
    _v;                         \
})

#define GET_AV_ID(_ast) ({      \
    ((AstVal*)(_ast))->val.str; \
})

#define GET_AV_STR(_ast) ({     \
    ((AstVal*)(_ast))->val.str; \
})

#define GET_AV_DOUBLE(_ast) ({ \
    ((AstVal*)(_ast))->val.d;  \
})

#define GET_AV_INT(_ast) ({     \
    ((AstVal*)(_ast))->val.num; \
})

#define GET_AV_TYPE(_ast) ({ \
    ((AstVal*)(_ast))->type; \
})

#define GET_AV_LEN(_ast) ({                    \
    int _size;                                 \
    if (GET_AV_TYPE(_ast) == AVT_DOUBLE) {     \
        _size = sizeof(double);                \
    } else if (GET_AV_TYPE(_ast) == AVT_INT) { \
        _size = sizeof(int);                   \
    } else {                                   \
        _size = strlen(GET_AV_STR(_ast)) + 1;  \
    }                                          \
    _size;                                     \
})

#define PRINT_AV(_ast)                             \
    do {                                           \
        if (GET_AV_TYPE(_ast) == AVT_DOUBLE) {     \
            printf("%g", GET_AV_DOUBLE(_ast));     \
        } else if (GET_AV_TYPE(_ast) == AVT_INT) { \
            printf("%d", GET_AV_INT(_ast));        \
        } else {                                   \
            printf("%s", GET_AV_STR(_ast));        \
        }                                          \
    } while (0);

Ast* create_ast(int children, AstKind kind, int attr, ...);
Ast* ast_add_child(Ast* a, Ast* child);
void print_ast(Ast* a);
void ast_destory(Ast* a);

Ast* G_AST;

#endif