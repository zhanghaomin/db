#include "include/ast.h"
#include "include/util.h"
#include <assert.h>
#include <stdarg.h>

Ast* G_AST = NULL;

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
        if (GET_AV_TYPE(a) == AVT_STR || GET_AV_TYPE(a) == AVT_ID) {
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