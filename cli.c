#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yyparse();
extern void yyrestart(FILE* input_file);

// void do_cmd(Ast* a)
// {

// }

int main(int argc, char const* argv[])
{
    (void)argv;
    (void)argc;
    printf("cmd > ");
    fflush(stdout);
    yyrestart(stdin);

    while (yyparse() == 0) {
        print_ast(G_AST, 1, 1);
        printf("cmd > ");
        fflush(stdout);
    }

    return 0;
}