#include "include/table.h"

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
    print_space(step, last, lasts);

    if (a == NULL) {
        printf("null\n");
        return;
    }

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
            print_ast2(a->child[i], step + 2, (i == a->children - 1), lasts);
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

void println_row_line(int* padding, int pn)
{
    int n = 0;
    for (int i = 0; i < pn; i++) {
        n += padding[i];
    }

    for (int i = 0; i < n + pn * 2 + 1; i++) {
        printf("-");
    }

    printf("\n");
}

void println_table_col_name(Table* t, int* padding)
{
    int len = 0;

    for (int i = 0; i < t->row_fmt->origin_cols_count; i++) {
        if (i == 0) {
            printf("| ");
        }
        len = strlen(t->row_fmt->origin_cols_name[i]);
        printf("%s", t->row_fmt->origin_cols_name[i]);
        for (int j = 0; j < padding[i] - len; j++) {
            printf(" ");
        }
        printf("| ");
    }

    printf("\n");
}

void println_row(QueryResult* qr, int qr_len, int* padding)
{
    int len = 0;
    char s[1024];

    for (int i = 0; i < qr_len; i++) {
        if (i == 0) {
            printf("| ");
        }

        if (qr[i]->type == C_VARCHAR || qr[i]->type == C_CHAR) {
            len = strlen((char*)(qr[i]->data));
            printf("%s", (char*)(qr[i]->data));
        } else if (qr[i]->type == C_INT) {
            itoa(*(int*)(qr[i]->data), s, 10);
            len = strlen(s);
            printf("%d", *(int*)(qr[i]->data));
        } else if (qr[i]->type == C_DOUBLE) {
            itoa(*(double*)(qr[i]->data), s, 10);
            len = strlen(s);
            printf("%f", *(double*)(qr[i]->data));
        }

        for (int j = 0; j < padding[i] - len; j++) {
            printf(" ");
        }

        printf("| ");
    }

    printf("\n");
}

void println_rows(QueryResultList* qrl, int qrl_len, int qr_len)
{
    int* paddings;
    paddings = scalloc(sizeof(int), qr_len);
    char s[1024];

    for (int i = 0; i < qrl_len; i++) {
        for (int j = 0; j < qr_len; j++) {
            if (qrl[i][j]->type == C_VARCHAR || qrl[i][j]->type == C_CHAR) {
                if (paddings[j] < (int)strlen(qrl[i][j]->data)) {
                    paddings[j] = (int)strlen(qrl[i][j]->data);
                }
            } else if (qrl[i][j]->type == C_INT || qrl[i][j]->type == C_DOUBLE) {
                if (qrl[i][j]->type == C_INT) {
                    itoa(*(int*)(qrl[i][j]->data), s, 10);
                } else {
                    itoa(*(double*)(qrl[i][j]->data), s, 10);
                }
                if (paddings[j] < (int)strlen(s)) {
                    paddings[j] = (int)strlen(s);
                }
            }
        }
    }

    for (int i = 0; i < qr_len; i++) {
        paddings[i] += 3;
    }

    println_row_line(paddings, qr_len);
    for (int i = 0; i < qrl_len; i++) {
        println_row(qrl[i], qr_len, paddings);
        println_row_line(paddings, qr_len);
    }

    free(paddings);
}