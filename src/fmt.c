#include "include/table.h"

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