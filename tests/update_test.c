#include "include/ast.h"
#include "include/table.h"
#include "include/util.h"
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
    char s[4096];
    DB* d;
    QueryResultList* qrl;
    int res, row_count, field_count;

    f = fopen("./tests/update_test.sql", "r+");
    d = db_init();

    lex_read_line(f, s, 4096);
    res = update_table(d, G_AST);
    assert(res == 1);
    ast_destory(G_AST);

    lex_read_line(f, s, 4096);
    qrl = select_row(d, G_AST, &row_count, &field_count, 1);
    assert(qrl != NULL);
    assert(row_count != 0);
    assert(field_count != 0);
    // println_rows(qrl, row_count, field_count);
    ast_destory(G_AST);
    destory_query_result_list(qrl, row_count, field_count);

    db_destory(d);
    fclose(f);
    printf("update test pass\n");
    return 0;
}
