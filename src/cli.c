#include "include/ast.h"
#include "include/linenoise.h"
#include "include/table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

extern int yyparse();
extern void yyrestart(FILE* input_file);
extern int lex_read(char* s, int len);

void do_cmd(DB* d, Ast* a)
{
    struct timeval t1, t2, tresult;
    double timeuse;
    gettimeofday(&t1, NULL);

    if (a->kind == AST_SELECT) {
        int row_count = 0;
        int field_count = 0;
        QueryResultList* qrl;

        qrl = select_row(d, G_AST, &row_count, &field_count, 1);
        if (qrl != NULL) {
            println_rows(qrl, row_count, field_count);
            destory_query_result_list(qrl, row_count, field_count);
        }

        ast_destory(G_AST);
    } else if (a->kind == AST_CREATE) {
        int res = 0;
        res = create_table(d, G_AST);
        if (res != 0) {
            printf("create table fail.\n");
        } else {
            printf("create table ok.\n");
        }
        ast_destory(G_AST);
    } else if (a->kind == AST_INSERT) {
        int res = 0;
        res = insert_row(d, G_AST);
        printf("%d effected rows.\n", res);
        ast_destory(G_AST);
    } else if (a->kind == AST_DELETE) {
        int res = 0;
        res = delete_row(d, G_AST);
        printf("%d effected rows.\n", res);
        ast_destory(G_AST);
    } else if (a->kind == AST_UPDATE) {
        int res = 0;
        res = update_table(d, G_AST);
        printf("%d effected rows.\n", res);
        ast_destory(G_AST);
    } else if (a->kind == AST_DROP_TABLE) {
        drop_table(d, G_AST);
        printf("delete ok.\n");
        ast_destory(G_AST);
    }

    gettimeofday(&t2, NULL);
    timersub(&t2, &t1, &tresult);
    timeuse = tresult.tv_sec * 1000 + (1.0 * tresult.tv_usec) / 1000;
    printf("query cost: %fms.\n", timeuse);
}

int main(int argc UNUSED, char const* argv[] UNUSED)
{
    DB* d;
    char* line;
    d = db_init();

    printf("hello\n");
    linenoiseHistoryLoad("history.txt");
    while ((line = linenoise("db> ")) != NULL) {
        /* Do something with the string. */
        if (line[0] != '\0' && line[0] != '/') {
            // 添加命令至历史列表
            linenoiseHistoryAdd(line); /* Add to the history. */
            // 保存命令至历史文件
            linenoiseHistorySave("history.txt"); /* Save the history on disk. */
            if (lex_read(line, strlen(line)) == 0) {
                do_cmd(d, G_AST);
            }
        }
        free(line);
    }

    db_destory(d);
    printf("bye\n");
    return 0;
}