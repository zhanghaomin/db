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
    char s[4096];
    DB* d;
    Table* t;
    QueryResultList* qrl;
    int res, row_count, field_count;

    f = fopen("./tests/delete_test.sql", "r+");
    d = db_init();
    t = open_table(d, "txzj_miniapp_white_list");

    lex_read_line(f, s, 4096);
    qrl = select_row(d, G_AST, &row_count, &field_count);
    assert(row_count == 2);
    ast_destory(G_AST);
    destory_query_result_list(qrl, row_count, field_count);

    lex_read_line(f, s, 4096);
    row_count = get_table_row_cnt(t);
    res = delete_row(d, G_AST);
    assert(res == 1);
    assert(get_table_row_cnt(t) == row_count - 1);
    ast_destory(G_AST);

    lex_read_line(f, s, 4096);
    qrl = select_row(d, G_AST, &row_count, &field_count);
    assert(row_count == 1);
    ast_destory(G_AST);
    destory_query_result_list(qrl, row_count, field_count);

    lex_read_line(f, s, 4096);
    qrl = select_row(d, G_AST, &row_count, &field_count);
    assert(row_count > 1);
    ast_destory(G_AST);
    destory_query_result_list(qrl, row_count, field_count);

    lex_read_line(f, s, 4096);
    res = delete_row(d, G_AST);
    assert(res != 0);
    assert(get_table_row_cnt(t) == 0);
    ast_destory(G_AST);

    lex_read_line(f, s, 4096);
    qrl = select_row(d, G_AST, &row_count, &field_count);
    assert(row_count == 1);
    ast_destory(G_AST);
    destory_query_result_list(qrl, row_count, field_count);

    db_destory(d);
    fclose(f);
    printf("delete test pass\n");
    return 0;
}
