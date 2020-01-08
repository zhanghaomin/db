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
    Table* t;
    int res, row_cnt, col_cnt;
    QueryResultList* qrl;

    f = fopen("./tests/drop_test.sql", "r+");
    d = db_init();
    t = open_table(d, "txzj_miniapp_white_list");

    assert(t != NULL);
    lex_read_line(f, s, 4096);
    res = drop_table(d, G_AST);
    assert(res == 0);
    ast_destory(G_AST);

    lex_read_line(f, s, 4096);
    qrl = select_row(d, G_AST, &row_cnt, &col_cnt, 0);
    assert(qrl == NULL);
    ast_destory(G_AST);

    lex_read_line(f, s, 4096);
    qrl = select_row(d, G_AST, &row_cnt, &col_cnt, 0);
    assert(qrl == NULL);
    ast_destory(G_AST);

    lex_read_line(f, s, 4096);
    qrl = select_row(d, G_AST, &row_cnt, &col_cnt, 0);
    assert(qrl == NULL);
    ast_destory(G_AST);

    t = open_table(d, "txzj_miniapp_white_list");
    assert(t == NULL);

    assert(access("t_txzj_miniapp_white_list.dat", 0) != 0);

    db_destory(d);
    fclose(f);
    printf("delete table test pass\n");
    return 0;
}
