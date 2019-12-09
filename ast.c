#include "ast.h"
#include "util.h"
#include <stdarg.h>

ast* create_ast(int children, ast_kind kind, int op, ...)
{
    va_list ap;
    ast* a;
    a = smalloc(sizeof(ast));
    a->children = children;
    a->kind = kind;
    a->op = op;
    a->child = smalloc(sizeof(ast*) * children);
    va_start(ap, op);

    for (int i = 0; i < children; i++) {
        a->child[i] = va_arg(ap, ast*);
    }

    va_end(ap);
    return a;
}

void ast_add_child(ast* a, ast* child)
{
    a->child = realloc(a->child, ++a->children);
    a->child[a->children - 1] = child;
}