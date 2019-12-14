#include "include/ast.h"
#include "include/table.h"
#include "include/util.h"
#include <assert.h>
#include <unistd.h>

extern int yyparse();
extern void yyrestart(FILE* input_file);
extern void lex_read_line(FILE* fp, char* s, int len);

void UNUSED assert_eq_str(char* str, char* str2)
{
    if (strcmp(str, str2) != 0) {
        printf("str not eq s1: %s, s2: %s\n", str, str2);
        exit(-1);
    }
}

int main(int argc UNUSED, char const* argv[] UNUSED)
{
    FILE* f;
    f = fopen("./tests/ast_test.sql", "r+");
    char s[4096];
    lex_read_line(f, s, 4096);
    print_ast(G_AST);
    ast_destory(G_AST);
    printf("ast pass\n");
    fclose(f);
    return 0;
}
