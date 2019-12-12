#include "ast.h"
#include "table.h"
#include "util.h"
#include <assert.h>
#include <unistd.h>

extern int yyparse();
extern void yyrestart(FILE* input_file);

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
    f = fopen("test_create_table.txt", "r+");
    yyrestart(f);
    assert(yyparse() == 0);

    DB* d;
    Table* t;
    int test_get = 0;
    d = db_init(NULL);
    t = create_table(d, G_AST);
retry:
    assert_eq_str(t->name, "txzj_miniapp_white_list");
    assert(t->data_fd > 0);
    assert(access("t_txzj_miniapp_white_list.dat", 0) == 0);
    assert(t->row_count == 0);
    assert(t->max_page_num == 0);

    for (int i = 0; i < MAX_PAGE_CNT_P_TABLE; i++) {
        assert(t->pager->pages[i] == NULL);
        assert(t->free_map[i] == PAGE_SIZE - sizeof(t->pager->pages[i]->header));
    }

    RowFmt* rf;
    rf = t->row_fmt;

    assert_eq_str(rf->origin_cols_name[0], "id");
    assert_eq_str(rf->static_cols_name[0], "id");
    assert(rf->cols_fmt[0].is_dynamic == 0);
    assert(rf->cols_fmt[0].len == sizeof(int));
    assert(rf->cols_fmt[0].type == C_INT);

    assert_eq_str(rf->origin_cols_name[1], "shop_id");
    assert_eq_str(rf->static_cols_name[1], "shop_id");
    assert(rf->cols_fmt[1].is_dynamic == 0);
    assert(rf->cols_fmt[1].len == sizeof(int));
    assert(rf->cols_fmt[1].type == C_INT);

    assert_eq_str(rf->origin_cols_name[2], "nick");
    assert_eq_str(rf->static_cols_name[2], "nick");
    assert(rf->cols_fmt[2].is_dynamic == 0);
    assert(rf->cols_fmt[2].len == 255);
    assert(rf->cols_fmt[2].type == C_CHAR);

    assert_eq_str(rf->origin_cols_name[3], "online_code");
    assert_eq_str(rf->dynamic_cols_name[0], "online_code");
    assert(rf->cols_fmt[3].is_dynamic == 1);
    assert(rf->cols_fmt[3].len == 256);
    assert(rf->cols_fmt[3].type == C_VARCHAR);

    assert(rf->dynamic_cols_count == 1);
    assert(rf->static_cols_count == 3);
    assert(rf->origin_cols_count == 4);

    if (!test_get) {
        printf("create table pass\n");
        t = open_table(d, "txzj_miniapp_white_list");
        test_get = 1;
        goto retry;
    }

    printf("open null table pass\n");

    f = fopen("test_insert_table.txt", "r+");
    yyrestart(f);
    yyparse();
    // print_ast(G_AST, 1, 1);
    assert(insert_row(d, G_AST) == 1);
    printf("insert pass\n");

    Cursor* c;
    c = cursor_init(t);
    traverse_table(c);
    printf("cursor pass\n");
    return 0;
}
