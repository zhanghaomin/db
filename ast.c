#include "ast.h"
#include "assert.h"
#include "util.h"
#include <stdarg.h>

static int lasts[20] = { 0 };

void print_space(int step, int last)
{
    for (int i = 1; i <= step; i++) {
        if (i == step) {
            if (last) {
                lasts[i] = last;
                printf("`-- ");
            } else {
                lasts[i] = last;
                printf("|-- ");
            }
        } else {
            if (lasts[i]) {
                printf("    ");
            } else {
                printf("|   ");
            }
        }
    }
}

void print_kind(int kind)
{
    switch (kind) {
    case AST_SELECT:
        printf("AST_SELECT:\n");
        break;
    case AST_EXPECT_COLS:
        printf("AST_EXPECT_COLS:\n");
        break;
    case AST_VAL:
        printf("AST_VAL:\n");
        break;
    case AST_WHERE_LEAF:
        printf("AST_WHERE_LEAF:\n");
        break;
    case AST_WHERE_NODE:
        printf("AST_WHERE_NODE:\n");
        break;
    case AST_TABLE:
        printf("AST_TABLE:\n");
        break;
    case AST_COL_FMT:
        printf("AST_COL_FMT:\n");
        break;
    case AST_COL_FMT_LIST:
        printf("AST_COL_FMT_LIST:\n");
        break;
    case AST_CREATE:
        printf("AST_CREATE:\n");
        break;
    default:
        break;
    }
}

void print_attr(int attr)
{
    if (attr == -1) {
        printf("attr: none\n");
        return;
    }

    printf("attr: %d\n", attr);
    // printf("")
    // switch (attr) {
    // case EQ:
    //     printf("EQ\n");
    //     break;
    // case W_AND:
    //     printf("AND\n");
    //     break;
    // case W_OR:
    //     printf("OR\n");
    //     break;
    // case -1:
    //     printf("none\n");
    //     break;
    // default:
    //     printf("%d\n", attr);
    //     break;
    // }
}

void print_ast_val(ast_val* av, int step)
{
    print_space(step + 1, 1);
    printf("val: ");

    if (av->val->type == INTEGER) {
        printf("%d\n", av->val->val.num);
    } else if (av->val->type == DOUBLE) {
        printf("%g\n", av->val->val.d);
    } else if (av->val->type == STRING) {
        printf("%s\n", av->val->val.str);
    }
}

void print_ast(ast* a, int step, int last)
{
    if (a == NULL) {
        return;
    }

    print_space(step, last);
    print_kind(a->kind);

    if (a->kind == AST_VAL) {
        print_ast_val((ast_val*)a, step);
        return;
    } else {
        print_space(step + 1, 0);
        print_attr(a->attr);
        print_space(step + 1, 0);
        printf("children: %d\n", a->children);
        print_space(step + 1, 1);
        printf("child: \n");
        for (int i = 0; i < a->children; i++) {
            print_ast(a->child[i], step + 2, i == a->children - 1);
        }
    }
}

ast* create_ast(int children, ast_kind kind, int attr, ...)
{
    va_list ap;
    ast* a;
    a = smalloc(sizeof(ast));
    a->kind = kind;
    a->attr = attr;

    if (child_is_unlimited(kind)) {
        a->children = children;
    } else {
        a->children = max_child_cnt(kind);
    }

    if (children == 0) {
        a->child = NULL;
        return a;
    }

    a->child = smalloc(sizeof(ast*) * children);
    va_start(ap, attr);

    for (int i = 0; i < children; i++) {
        a->child[i] = va_arg(ap, ast*);
    }

    va_end(ap);
    return a;
}

void ast_add_child(ast* a, ast* child)
{
    assert(child_is_unlimited(a->kind));

    if (a->child == NULL) {
        a->child = smalloc(sizeof(ast*) * ++a->children);
    } else {
        a->child = realloc(a->child, ++a->children * sizeof(ast*));
    }

    a->child[a->children - 1] = child;
}