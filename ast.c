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

void print_attr(int attr)
{
    if (attr == -1) {
        printf("attr: none\n");
        return;
    }

    printf("attr: %d\n", attr);
}

void print_ast_val(AstVal* av, int step)
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

void print_ast(Ast* a, int step, int last)
{
    if (a == NULL) {
        return;
    }

    print_space(step, last);
    printf("%s:\n", get_kind_name(a->kind));

    if (a->kind == AST_VAL) {
        print_ast_val((AstVal*)a, step);
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

Ast* create_ast(int children, AstKind kind, int attr, ...)
{
    va_list ap;
    Ast* a;
    a = smalloc(sizeof(Ast));
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

    a->child = smalloc(sizeof(Ast*) * children);
    va_start(ap, attr);

    for (int i = 0; i < children; i++) {
        a->child[i] = va_arg(ap, Ast*);
    }

    va_end(ap);
    return a;
}

Ast* ast_add_child(Ast* a, Ast* child)
{
    assert(child_is_unlimited(a->kind));

    if (a->child == NULL) {
        a->child = smalloc(sizeof(Ast*) * ++a->children);
    } else {
        a->child = realloc(a->child, ++a->children * sizeof(Ast*));
    }

    a->child[a->children - 1] = child;
    return a;
}