#include "include/ast.h"
#include "include/util.h"
#include <assert.h>
#include <stdarg.h>

void print_space(int step, int last, int lasts[])
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

void print_ast_val(AstVal* av, int step, int lasts[])
{
    print_space(step + 1, 1, lasts);
    printf("val: ");

    PRINT_AV(av);
    printf("\n");
}

void print_ast2(Ast* a, int step, int last, int lasts[])
{
    if (a == NULL) {
        return;
    }

    print_space(step, last, lasts);
    printf("%s:\n", get_kind_name(a->kind));

    if (a->kind == AST_VAL) {
        print_ast_val((AstVal*)a, step, lasts);
        return;
    } else {
        print_space(step + 1, 0, lasts);
        print_attr(a->attr);
        print_space(step + 1, 0, lasts);
        printf("children: %d\n", a->children);
        print_space(step + 1, 1, lasts);
        printf("child: \n");
        for (int i = 0; i < a->children; i++) {
            print_ast2(a->child[i], step + 2, (i == a->children - 1) || (a->child[i + 1] == NULL), lasts);
        }
    }
}

void print_ast(Ast* a)
{
    int* lasts;
    lasts = scalloc(sizeof(int) * 100, 1);
    print_ast2(a, 1, 1, lasts);
    free(lasts);
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

    a->child = scalloc(sizeof(Ast*) * children, 1);
    va_start(ap, attr);

    for (int i = 0; i < children; i++) {
        a->child[i] = va_arg(ap, Ast*);
    }

    va_end(ap);
    return a;
}

void ast_destory(Ast* a)
{
    if (a == NULL) {
        return;
    }

    if (a->kind == AST_VAL) {
        if (GET_AV_TYPE(a) == AVT_STR) {
            free(GET_AV_STR(a));
        }
        free(a);
        return;
    }

    for (int i = 0; i < a->children; i++) {
        if (a->child[i] != NULL) {
            ast_destory(a->child[i]);
        }
    }

    free(a->child);
    free(a);
}

Ast* ast_add_child(Ast* a, Ast* child)
{
    assert(child_is_unlimited(a->kind));
    a->child = realloc(a->child, ++a->children * sizeof(Ast*));
    a->child[a->children - 1] = child;
    return a;
}