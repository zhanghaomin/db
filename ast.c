#include "ast.h"
#include "util.h"
#include <stdarg.h>

void print_space(int space_count)
{
    for (int i = 0; i < space_count; i++) {
        printf(" ");
    }
}

void print_kind(int kind, int space_count)
{
    print_space(space_count);
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

void print_attr(int attr, int space_count)
{
    print_space(space_count);

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

void print_ast_val(ast_val* av, int space_count)
{
    print_kind(av->kind, space_count);
    print_space(space_count + 4);
    printf("val: ");

    if (av->val->type == INTEGER) {
        printf("%d\n", av->val->val.num);
    } else if (av->val->type == DOUBLE) {
        printf("%g\n", av->val->val.d);
    } else if (av->val->type == STRING) {
        printf("%s\n", av->val->val.str);
    }
}

void print_ast(ast* a, int space_count)
{
    if (a->kind == AST_VAL) {
        print_ast_val((ast_val*)a, space_count);
        return;
    } else {
        print_kind(a->kind, space_count);
        print_attr(a->attr, space_count + 4);
        print_space(space_count + 4);
        printf("children: %d\n", a->children);
        print_space(space_count + 4);
        printf("child: \n");
        for (int i = 0; i < a->children; i++) {
            print_ast(a->child[i], space_count + 8);
        }
    }
}

ast* create_ast(int children, ast_kind kind, int attr, ...)
{
    va_list ap;
    ast* a;
    a = smalloc(sizeof(ast));
    a->children = children;
    a->kind = kind;
    a->attr = attr;

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
    if (a->child == NULL) {
        a->child = smalloc(sizeof(ast*) * ++a->children);
    } else {
        a->child = realloc(a->child, ++a->children * sizeof(ast*));
    }

    a->child[a->children - 1] = child;
}