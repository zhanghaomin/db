#include "include/table.h"
#include "include/util.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int get_where_expr_res(Table* t, Page* p, int dir_num, Ast* where_expr);
static QueryResultVal* get_expr_res(Table* t, Page* p, int dir_num, Ast* expr);
static QueryResultVal* av_to_qrv(Ast* a);
static int execute_update_sql(DB* d, char* sql);
static QueryResultList* execute_select_sql(DB* d, char* sql, int* row_cnt, int* col_cnt);
int lex_read(char* s, int len);
int get_sys_tables_max_page_num(Table* t);
int get_sys_tables_row_count(Table* t);
static int qrv_to_int(QueryResultVal* qrv);

char* SYS_TABLES = "sys_tables";
char* SYS_COLS = "sys_cols";
char* SYS_FREE_SPACE = "sys_free_space";

void destory_query_result_list(QueryResultList* qrl, int qrl_len, int qr_len)
{
    for (int i = 0; i < qrl_len; i++) {
        for (int j = 0; j < qr_len; j++) {
            free(qrl[i][j]->data);
            free(qrl[i][j]);
        }

        free(qrl[i]);
    }

    free(qrl);
}

void destory_query_result(QueryResult* qr, int qr_len)
{
    for (int i = 0; i < qr_len; i++) {
        free(qr[i]->data);
        free(qr[i]);
    }

    free(qr);
}

QueryResult* get_table_header(Table* t)
{
    QueryResult* qr = NULL;

    for (int j = 0; j < t->row_fmt->origin_cols_count; j++) {
        qr = realloc(qr, sizeof(QueryResultVal*) * (j + 1));
        qr[j] = smalloc(sizeof(QueryResultVal));
        qr[j]->data = strdup(t->row_fmt->origin_cols_name[j]);
        qr[j]->type = C_CHAR;
    }

    return qr;
}

int get_dynamic_col_num_by_col_name(RowFmt* rf, char* col_name)
{
    for (int i = 0; i < rf->dynamic_cols_count; i++) {
        if (strcmp(rf->dynamic_cols_name[i], col_name) == 0) {
            return i;
        }
    }

    return -1;
}

int get_col_num_by_col_name(RowFmt* rf, char* col_name)
{
    for (int i = 0; i < rf->origin_cols_count; i++) {
        if (strcmp(rf->origin_cols_name[i], col_name) == 0) {
            return i;
        }
    }

    return -1;
}

inline int get_query_result_val_len(QueryResultVal* qrv)
{
    if (qrv->type == C_INT) {
        return sizeof(int);
    } else if (qrv->type == C_DOUBLE) {
        return sizeof(double);
    } else if (qrv->type == C_CHAR || qrv->type == C_VARCHAR) {
        return strlen(qrv->data) + 1;
    }

    return 0;
}

static void get_table_file_name(const char* name, char* file_name)
{
    sprintf(file_name, "t_%s.dat", name);
}

void destory_query_result_val(QueryResultVal* qrv)
{
    free(qrv->data);
    free(qrv);
}

// sys_tables: char(255) name int(11) max_page_num int(11) row_count
static void init_sys_tables_row_fmt(RowFmt* rf)
{
    rf->origin_cols_name = smalloc(sizeof(char*) * 3);
    rf->origin_cols_name[0] = strdup("name");
    rf->origin_cols_name[1] = strdup("max_page_num");
    rf->origin_cols_name[2] = strdup("row_count");
    rf->origin_cols_count = 3;
    rf->static_cols_name = smalloc(sizeof(char*) * 3);
    rf->static_cols_name[0] = strdup("name");
    rf->static_cols_name[1] = strdup("max_page_num");
    rf->static_cols_name[2] = strdup("row_count");
    rf->static_cols_count = 3;
    rf->dynamic_cols_count = 0;
    rf->dynamic_cols_name = NULL;
    rf->cols_fmt = smalloc(sizeof(ColFmt) * 3);
    rf->cols_fmt[0].is_dynamic = 0;
    rf->cols_fmt[0].type = C_CHAR;
    rf->cols_fmt[0].len = 255;
    rf->cols_fmt[1].is_dynamic = 0;
    rf->cols_fmt[1].type = C_INT;
    rf->cols_fmt[1].len = 11;
    rf->cols_fmt[2].is_dynamic = 0;
    rf->cols_fmt[2].type = C_INT;
    rf->cols_fmt[2].len = 11;
}

// sys_free_space: table_name page_num free
static void init_sys_free_space_row_fmt(RowFmt* rf)
{
    rf->origin_cols_name = smalloc(sizeof(char*) * 3);
    rf->origin_cols_name[0] = strdup("table_name");
    rf->origin_cols_name[1] = strdup("page_num");
    rf->origin_cols_name[2] = strdup("free");
    rf->static_cols_name = smalloc(sizeof(char*) * 3);
    rf->static_cols_name[0] = strdup("table_name");
    rf->static_cols_name[1] = strdup("page_num");
    rf->static_cols_name[2] = strdup("free");
    rf->origin_cols_count = 3;
    rf->static_cols_count = 3;
    rf->dynamic_cols_count = 0;
    rf->dynamic_cols_name = NULL;
    rf->cols_fmt = smalloc(sizeof(ColFmt) * 3);
    rf->cols_fmt[0].is_dynamic = 0;
    rf->cols_fmt[0].type = C_CHAR;
    rf->cols_fmt[0].len = 255;
    rf->cols_fmt[1].is_dynamic = 0;
    rf->cols_fmt[1].type = C_INT;
    rf->cols_fmt[1].len = 11;
    rf->cols_fmt[2].is_dynamic = 0;
    rf->cols_fmt[2].type = C_INT;
    rf->cols_fmt[2].len = 11;
}

// sys_cols: table_name col_name origin_col_num col_type len
static void init_sys_clos_row_fmt(RowFmt* rf)
{
    rf->origin_cols_name = smalloc(sizeof(char*) * 5);
    rf->origin_cols_name[0] = strdup("table_name");
    rf->origin_cols_name[1] = strdup("col_name");
    rf->origin_cols_name[2] = strdup("origin_col_num");
    rf->origin_cols_name[3] = strdup("col_type");
    rf->origin_cols_name[4] = strdup("len");
    rf->static_cols_name = smalloc(sizeof(char*) * 5);
    rf->static_cols_name[0] = strdup("table_name");
    rf->static_cols_name[1] = strdup("col_name");
    rf->static_cols_name[2] = strdup("origin_col_num");
    rf->static_cols_name[3] = strdup("col_type");
    rf->static_cols_name[4] = strdup("len");
    rf->origin_cols_count = 5;
    rf->static_cols_count = 5;
    rf->dynamic_cols_count = 0;
    rf->dynamic_cols_name = NULL;
    rf->cols_fmt = smalloc(sizeof(ColFmt) * 5);
    rf->cols_fmt[0].is_dynamic = 0;
    rf->cols_fmt[0].type = C_CHAR;
    rf->cols_fmt[0].len = 255;
    rf->cols_fmt[1].is_dynamic = 0;
    rf->cols_fmt[1].type = C_CHAR;
    rf->cols_fmt[1].len = 255;
    rf->cols_fmt[2].is_dynamic = 0;
    rf->cols_fmt[2].type = C_INT;
    rf->cols_fmt[2].len = 11;
    rf->cols_fmt[3].is_dynamic = 0;
    rf->cols_fmt[3].type = C_INT;
    rf->cols_fmt[3].len = 11;
    rf->cols_fmt[4].is_dynamic = 0;
    rf->cols_fmt[4].type = C_INT;
    rf->cols_fmt[4].len = 11;
}

int incr_table_row_cnt(Table* t, int step)
{
    char sql[1024];

    if (step > 0) {
        sprintf(sql, "update %s set row_count = row_count + %d where name = '%s';", SYS_TABLES, step, t->name);
    } else {
        sprintf(sql, "update %s set row_count = row_count - %d where name = '%s';", SYS_TABLES, -step, t->name);
    }

    return execute_update_sql(t->db, sql);
}

int get_sys_tables_row_count(Table* t)
{
    QueryResultVal* qrv;
    int res;
    qrv = get_col_val(get_page(t, 0), 0, t->row_fmt, "row_count");
    res = qrv_to_int(qrv);
    destory_query_result_val(qrv);
    return res;
}

int get_sys_tables_max_page_num(Table* t)
{
    QueryResultVal* qrv;
    int res;
    qrv = get_col_val(get_page(t, 0), 0, t->row_fmt, "max_page_num");
    res = qrv_to_int(qrv);
    destory_query_result_val(qrv);
    return res;
}

inline int get_table_row_cnt(Table* t)
{
    int row_cnt, col_cnt, res;
    char sql[1024];
    QueryResultList* qrl;

    if (strcmp(t->name, SYS_TABLES) == 0) {
        return get_sys_tables_row_count(t);
    }

    sprintf(sql, "select row_count from %s where name = '%s';", SYS_TABLES, t->name);

    qrl = execute_select_sql(t->db, sql, &row_cnt, &col_cnt);

    assert(qrl != NULL);

    res = qrv_to_int(qrl[0][0]);
    destory_query_result_list(qrl, row_cnt, col_cnt);
    return res;
}

int get_table_max_page_num(Table* t)
{
    int row_cnt, col_cnt, res;
    char sql[1024];
    QueryResultList* qrl;

    if (strcmp(t->name, SYS_TABLES) == 0) {
        return get_sys_tables_max_page_num(t);
    }

    sprintf(sql, "select max_page_num from %s where name = '%s';", SYS_TABLES, t->name);
    qrl = execute_select_sql(t->db, sql, &row_cnt, &col_cnt);
    assert(qrl != NULL);
    res = qrv_to_int(qrl[0][0]);
    destory_query_result_list(qrl, row_cnt, col_cnt);
    return res;
}

int update_table_max_page_num(Table* t, int max_page_num)
{
    char sql[1024];
    sprintf(sql, "update %s set max_page_num = %d where name = '%s';", SYS_TABLES, max_page_num, t->name);
    return execute_update_sql(t->db, sql);
}

int insert_table_free_space(Table* t, int page_num, int free_space)
{
    char sql[1024];
    sprintf(sql, "insert into %s values( '%s', %d, %d);", SYS_FREE_SPACE, t->name, page_num, free_space);
    return execute_update_sql(t->db, sql);
}

int update_table_free_space(Table* t, int page_num, int free_space)
{
    char sql[1024];
    sprintf(sql, "update %s set free_space = %d where table_name = '%s' and page_num = %d;", SYS_FREE_SPACE, free_space, t->name, page_num);
    return execute_update_sql(t->db, sql);
}

static int add_row_to_table(Table* t, QueryResult* qr)
{
    int res;
    res = serialize_row(t, t->row_fmt, qr);
    incr_table_row_cnt(t, 1);
    return res;
}

static QueryResult* parse_insert_row(RowFmt* rf, Ast* a)
{
    assert(a->kind == AST_INSERT_VAL_LIST);

    QueryResult* qr;

    qr = smalloc(sizeof(QueryResultVal*) * rf->origin_cols_count);

    for (int i = 0; i < rf->origin_cols_count; i++) {
        qr[i] = av_to_qrv(a->child[i]);
    }

    return qr;
}

/**
 *  cvs必须跟列定义时的顺序一致, 不支持null
 */
int insert_row(DB* d, Ast* a)
{
    assert(a->kind == AST_INSERT);

    int res = 0;
    Table* t;
    Ast *table_name, *row_list, *val_list;
    QueryResult* qr;

    table_name = a->child[0];
    row_list = a->child[1];

    if ((t = open_table(d, GET_AV_STR(table_name->child[0]))) == NULL) {
        printf("table %s not exist.\n", GET_AV_STR(table_name->child[0]));
        return 0;
    }

    for (int i = 0; i < row_list->children; i++) {
        val_list = row_list->child[i];

        if (val_list->children != t->row_fmt->origin_cols_count) {
            printf("expect %d cols, but given %d rows.", t->row_fmt->origin_cols_count, val_list->children);
            break;
        }

        qr = parse_insert_row(t->row_fmt, val_list);
        res += add_row_to_table(t, qr);
        destory_query_result(qr, t->row_fmt->origin_cols_count);
    }

    return res;
}

static int qrv_to_bool(QueryResultVal* qrv)
{
    int i;
    double d;

    if (qrv->type == C_INT) {
        memcpy(&i, qrv->data, sizeof(int));
        return i != 0;
    } else if (qrv->type == C_DOUBLE) {
        memcpy(&d, qrv->data, sizeof(double));
        return d != (double)0;
    }

    // char varchar
    i = atoi(qrv->data);
    return i != 0;
}

static int qrv_to_int(QueryResultVal* qrv)
{
    int i;
    double d;

    if (qrv->type == C_INT) {
        memcpy(&i, qrv->data, sizeof(int));
        return i;
    } else if (qrv->type == C_DOUBLE) {
        memcpy(&d, qrv->data, sizeof(double));
        return (int)d;
    }

    // char varchar
    i = atoi(qrv->data);
    return i;
}

static double qrv_to_double(QueryResultVal* qrv)
{
    int i;
    double d;

    if (qrv->type == C_INT) {
        memcpy(&i, qrv->data, sizeof(int));
        return (double)i;
    } else if (qrv->type == C_DOUBLE) {
        memcpy(&d, qrv->data, sizeof(double));
        return d;
    }

    // char varchar
    d = atof(qrv->data);
    return d;
}

static QueryResultVal* do_compare_int(int op1, int op2, ExprOp op)
{
    QueryResultVal* qrv;
    int res = 0;

    if (op == E_EQ) {
        res = op1 == op2;
    } else if (op == E_GT) {
        res = op1 > op2;
    } else if (op == E_GTE) {
        res = op1 >= op2;
    } else if (op == E_LT) {
        res = op1 < op2;
    } else if (op == E_LTE) {
        res = op1 <= op2;
    } else if (op == E_NEQ) {
        res = op1 != op2;
    }

    qrv = smalloc(sizeof(QueryResultVal));
    qrv->type = C_INT;
    qrv->data = smalloc(sizeof(int));
    memcpy(qrv->data, &res, sizeof(int));
    return qrv;
}

static QueryResultVal* do_compare_double(double op1, double op2, ExprOp op)
{
    QueryResultVal* qrv;
    int res = 0;

    if (op == E_EQ) {
        res = op1 == op2;
    } else if (op == E_GT) {
        res = op1 > op2;
    } else if (op == E_GTE) {
        res = op1 >= op2;
    } else if (op == E_LT) {
        res = op1 < op2;
    } else if (op == E_LTE) {
        res = op1 <= op2;
    } else if (op == E_NEQ) {
        res = op1 != op2;
    }

    qrv = smalloc(sizeof(QueryResultVal));
    qrv->type = C_INT;
    qrv->data = smalloc(sizeof(int));
    memcpy(qrv->data, &res, sizeof(int));
    return qrv;
}

static QueryResultVal* do_compare_str(char* op1, char* op2, ExprOp op)
{
    QueryResultVal* qrv;
    int res = 0;
    res = strcmp(op1, op2);
    if (op == E_EQ) {
        res = res == 0;
    } else if (op == E_GT) {
        res = res > 0;
    } else if (op == E_GTE) {
        res = res >= 0;
    } else if (op == E_LT) {
        res = res < 0;
    } else if (op == E_LTE) {
        res = res <= 0;
    } else if (op == E_NEQ) {
        res = res != 0;
    }

    qrv = smalloc(sizeof(QueryResultVal));
    qrv->type = C_INT;
    qrv->data = smalloc(sizeof(int));
    memcpy(qrv->data, &res, sizeof(int));
    return qrv;
}

static QueryResultVal* do_bool(int op1, int op2, ExprOp op)
{
    QueryResultVal* qrv;
    int res = 0;

    if (op == E_AND) {
        res = op1 && op2;
    } else if (op == E_OR) {
        res = op1 || op2;
    } else if (op == E_NOT) {
        res = !op1;
    }

    qrv = smalloc(sizeof(QueryResultVal));
    qrv->type = C_INT;
    qrv->data = smalloc(sizeof(int));
    memcpy(qrv->data, &res, sizeof(int));
    return qrv;
}

static QueryResultVal* do_calc_int(int op1, int op2, ExprOp op)
{
    QueryResultVal* qrv;
    int res = 0;

    if (op == E_MOD) {
        res = op1 % op2;
    } else if (op == E_MUL) {
        res = op1 * op2;
    } else if (op == E_SUB) {
        res = op1 - op2;
    } else if (op == E_ADD) {
        res = op1 + op2;
    } else if (op == E_DIV) {
        if (op2 == 0) {
            printf("divide by 0");
            res = 0;
        } else {
            res = op1 / op2;
        }
    } else if (op == E_B_AND) {
        res = op1 & op2;
    } else if (op == E_B_OR) {
        res = op1 | op2;
    } else if (op == E_B_NOT) {
        res = ~op1;
    } else if (op == E_B_XOR) {
        res = op1 ^ op2;
    }

    qrv = smalloc(sizeof(QueryResultVal));
    qrv->type = C_INT;
    qrv->data = smalloc(sizeof(int));
    memcpy(qrv->data, &res, sizeof(int));
    return qrv;
}

static QueryResultVal* do_calc_double(double op1, double op2, ExprOp op)
{
    QueryResultVal* qrv;
    double res = 0;

    if (op == E_MUL) {
        res = op1 * op2;
    } else if (op == E_SUB) {
        res = op1 - op2;
    } else if (op == E_ADD) {
        res = op1 + op2;
    } else if (op == E_DIV) {
        if (op2 == 0) {
            printf("divide by 0");
            res = 0;
        } else {
            res = op1 / op2;
        }
    }

    qrv = smalloc(sizeof(QueryResultVal));
    qrv->type = C_INT;
    qrv->data = smalloc(sizeof(int));
    memcpy(qrv->data, &res, sizeof(int));
    return qrv;
}

static QueryResultVal* cmd_bool(Table* t, Page* p, int dir_num, Ast* op1, Ast* op2, ExprOp op)
{
    QueryResultVal *op1_qrv, *op2_qrv, *res_qrv;

    op1_qrv = get_expr_res(t, p, dir_num, op1);

    if (op != E_NOT) {
        op2_qrv = get_expr_res(t, p, dir_num, op2);
        res_qrv = do_bool(qrv_to_bool(op1_qrv), qrv_to_bool(op2_qrv), op);
        destory_query_result_val(op1_qrv);
        destory_query_result_val(op2_qrv);
    } else {
        res_qrv = do_bool(qrv_to_bool(op1_qrv), 0, op);
        destory_query_result_val(op1_qrv);
    }

    return res_qrv;
}

static QueryResultVal* cmd_cacl(Table* t, Page* p, int dir_num, Ast* op1, Ast* op2, ExprOp op)
{
    QueryResultVal *op1_qrv, *op2_qrv, *res_qrv;

    op1_qrv = get_expr_res(t, p, dir_num, op1);

    if (op == E_B_NOT) {
        destory_query_result_val(op1_qrv);
        return do_calc_int(qrv_to_int(op1_qrv), 0, op);
    }

    op2_qrv = get_expr_res(t, p, dir_num, op2);

    if (op == E_B_AND || op == E_B_OR || op == E_B_XOR) {
        res_qrv = do_calc_int(qrv_to_int(op1_qrv), qrv_to_int(op2_qrv), op);
    } else {
        if (op1_qrv->type == C_DOUBLE || op2_qrv->type == C_DOUBLE) {
            res_qrv = do_calc_double(qrv_to_double(op1_qrv), qrv_to_double(op2_qrv), op);
        } else {
            res_qrv = do_calc_int(qrv_to_int(op1_qrv), qrv_to_int(op2_qrv), op);
        }
    }

    destory_query_result_val(op1_qrv);
    destory_query_result_val(op2_qrv);
    return res_qrv;
}

// 如果一个是字符串，一个是数字，那么都转成数字比较
static QueryResultVal* cmd_compare(Table* t, Page* p, int dir_num, Ast* op1, Ast* op2, ExprOp op)
{
    QueryResultVal *op1_qrv, *op2_qrv, *res_qrv;

    op1_qrv = get_expr_res(t, p, dir_num, op1);
    op2_qrv = get_expr_res(t, p, dir_num, op2);

    if (op1_qrv->type == C_DOUBLE || op2_qrv->type == C_DOUBLE) {
        res_qrv = do_compare_double(qrv_to_double(op1_qrv), qrv_to_double(op2_qrv), op);
    } else if (op1_qrv->type == C_INT || op2_qrv->type == C_INT) {
        res_qrv = do_compare_int(qrv_to_int(op1_qrv), qrv_to_int(op2_qrv), op);
    } else {
        res_qrv = do_compare_str(op1_qrv->data, op2_qrv->data, op);
    }

    destory_query_result_val(op1_qrv);
    destory_query_result_val(op2_qrv);
    return res_qrv;
}

static QueryResultVal* av_to_qrv(Ast* a)
{
    assert(a->kind == AST_VAL && GET_AV_TYPE(a) != AVT_ID);

    int i;
    double d;
    QueryResultVal* qrv;

    if (GET_AV_TYPE(a) == AVT_DOUBLE) {
        qrv = smalloc(sizeof(QueryResultVal));
        qrv->type = C_DOUBLE;
        qrv->data = smalloc(sizeof(double));
        d = GET_AV_DOUBLE(a);
        memcpy(qrv->data, &d, GET_AV_LEN(a));
    } else if (GET_AV_TYPE(a) == AVT_INT) {
        qrv = smalloc(sizeof(QueryResultVal));
        qrv->type = C_INT;
        qrv->data = smalloc(sizeof(int));
        i = GET_AV_INT(a);
        memcpy(qrv->data, &i, GET_AV_LEN(a));
    } else {
        // AVT_STR
        qrv = smalloc(sizeof(QueryResultVal));
        qrv->type = C_CHAR;
        qrv->data = smalloc(GET_AV_LEN(a));
        memcpy(qrv->data, GET_AV_STR(a), GET_AV_LEN(a));
    }

    return qrv;
}

static QueryResultVal* get_expr_res(Table* t, Page* p, int dir_num, Ast* expr)
{
    assert(expr->kind == AST_EXP || expr->kind == AST_VAL);

    Ast *op1, *op2;
    ExprOp op;
    QueryResultVal* qrv;

    op = expr->attr;

    if (expr->kind == AST_VAL) {
        if (GET_AV_TYPE(expr) == AVT_ID) {
            qrv = get_col_val(p, dir_num, t->row_fmt, GET_AV_STR(expr));
        } else {
            qrv = av_to_qrv(expr);
        }

        return qrv;
    }

    op1 = expr->child[0];
    op2 = expr->child[1];

    if (op == E_EQ || op == E_NEQ || op == E_GT || op == E_GTE || op == E_LT || op == E_LTE) {
        return cmd_compare(t, p, dir_num, op1, op2, op);
    } else if (op == E_NOT || op == E_OR || op == E_AND) {
        return cmd_bool(t, p, dir_num, op1, op2, op);
    }

    return cmd_cacl(t, p, dir_num, op1, op2, op);
}

static int get_where_expr_res(Table* t, Page* p, int dir_num, Ast* where_expr)
{
    assert(where_expr == NULL || where_expr->kind == AST_WHERE_EXP);

    int res;
    QueryResultVal* res_qrv;

    if (where_expr == NULL) {
        return 1;
    }

    res_qrv = get_expr_res(t, p, dir_num, where_expr->child[0]);
    res = qrv_to_bool(res_qrv);
    destory_query_result_val(res_qrv);
    return res;
}

QueryResult* get_table_row(Table* t, Page* p, int dir_num, int* col_cnt)
{
    QueryResult* qr;
    qr = smalloc(t->row_fmt->origin_cols_count * sizeof(QueryResultVal*));

    for (int i = 0; i < t->row_fmt->origin_cols_count; i++) {
        qr[i] = get_col_val(p, dir_num, t->row_fmt, t->row_fmt->origin_cols_name[i]);
    }

    if (col_cnt != NULL) {
        *col_cnt = t->row_fmt->origin_cols_count;
    }

    return qr;
}

QueryResult* filter_row(Table* t, Cursor* c, Ast* expect_cols, Ast* where_top_expr)
{
    QueryResult* qr;
    QueryResultVal* qrv;
    QueryResult* full_row;
    int col_cnt = 0;
    int table_row_count = 0;

    while (!cursor_is_end(c)) {
        if (!cursor_value_is_deleted(c) && get_where_expr_res(t, cursor_page(c), cursor_dir_num(c), where_top_expr)) {
            qr = smalloc(expect_cols->children * sizeof(QueryResultVal*));
            // 符合条件,返回这条数据
            for (int i = 0; i < expect_cols->children; i++) {
                if (GET_AV_TYPE(expect_cols->child[i]) != AVT_ID) {
                    qr[col_cnt++] = av_to_qrv(expect_cols->child[i]);
                } else {
                    if (strcmp(GET_AV_STR(expect_cols->child[i]), "*") == 0) {
                        full_row = get_table_row(t, cursor_page(c), cursor_dir_num(c), &table_row_count);

                        for (int i = 0; i < table_row_count; i++) {
                            qr = realloc(qr, ++col_cnt * sizeof(QueryResultVal*));
                            qr[col_cnt - 1] = full_row[i];
                        }

                        free(full_row);
                    } else {
                        qrv = get_col_val(cursor_page(c), cursor_dir_num(c), t->row_fmt, GET_AV_STR(expect_cols->child[i]));
                        qr[col_cnt++] = qrv;
                    }
                }
            }

            // 移动指针
            cursor_next(c);
            return qr;
        }

        // 继续下一条
        cursor_next(c);
    }

    return NULL;
}

int check_and_get_select_col_count(Table* t, Ast* expect_cols)
{
    assert(expect_cols->kind == AST_EXPECT_COLS);
    int count = 0;

    for (int i = 0; i < expect_cols->children; i++) {
        if (GET_AV_TYPE(expect_cols->child[i]) == AVT_ID) {
            if (strcmp(GET_AV_STR(expect_cols->child[i]), "*") == 0) {
                count += t->row_fmt->origin_cols_count;
                continue;
            }
            if (get_col_num_by_col_name(t->row_fmt, GET_AV_STR(expect_cols->child[i])) == -1) {
                printf("col `%s` not exist.\n", GET_AV_STR(expect_cols->child[i]));
                return -1;
            }
        }

        count++;
    }

    return count;
}

QueryResult* get_select_header(Table* t, Ast* expect_cols)
{
    assert(expect_cols->kind == AST_EXPECT_COLS);
    int count = 0;
    QueryResult* qr = NULL;

    for (int i = 0; i < expect_cols->children; i++) {
        if (GET_AV_TYPE(expect_cols->child[i]) == AVT_ID && strcmp(GET_AV_STR(expect_cols->child[i]), "*") == 0) {
            for (int j = 0; j < t->row_fmt->origin_cols_count; j++) {
                qr = realloc(qr, sizeof(QueryResultVal*) * ++count);
                qr[count - 1] = smalloc(sizeof(QueryResultVal));
                qr[count - 1]->data = strdup(t->row_fmt->origin_cols_name[j]);
                qr[count - 1]->type = C_CHAR;
            }
        } else {
            qr = realloc(qr, sizeof(QueryResultVal*) * ++count);
            qr[count - 1] = smalloc(sizeof(QueryResultVal));
            qr[count - 1]->data = strdup(GET_AV_STR(expect_cols->child[i]));
            qr[count - 1]->type = C_CHAR;
        }
    }

    return qr;
}

int check_where_ast_valid(Table* t, Ast* where_expr)
{
    if (where_expr == NULL) {
        return 1;
    }

    assert(where_expr->kind == AST_EXP || where_expr->kind == AST_VAL);

    if (where_expr->kind == AST_VAL) {
        if (GET_AV_TYPE(where_expr) == AVT_ID && get_col_num_by_col_name(t->row_fmt, GET_AV_STR(where_expr)) == -1) {
            printf("col `%s` not exist.\n", GET_AV_STR(where_expr));
            return 0;
        }
        return 1;
    }

    return check_where_ast_valid(t, where_expr->child[0]) && check_where_ast_valid(t, where_expr->child[1]);
}

QueryResultList* select_row(DB* d, Ast* select_ast, int* row_count, int* col_count, int with_header)
{
    assert(select_ast->kind == AST_SELECT);

    Ast *table_name, *expect_cols, *where_top_list, *order_by, *limit;
    Table* t;
    QueryResultList* qrl = NULL;
    QueryResult* qr;
    Cursor* c;
    int offset = 0;
    int lmt = -1;

    expect_cols = select_ast->child[0];
    table_name = select_ast->child[1];
    where_top_list = select_ast->child[2];
    order_by = select_ast->child[3];
    limit = select_ast->child[4];

    t = open_table(d, GET_AV_STR(table_name->child[0]));

    if (t == NULL) {
        printf("table `%s` not exits.\n", GET_AV_STR(table_name->child[0]));
        return NULL;
    }

    c = cursor_init(t);
    (*row_count) = 0;
    *col_count = check_and_get_select_col_count(t, expect_cols);

    if (*col_count == -1) {
        *row_count = 0;
        return NULL;
    }

    if (where_top_list != NULL && !check_where_ast_valid(t, where_top_list->child[0])) {
        return NULL;
    }

    if (limit != NULL) {
        offset = GET_AV_INT(limit->child[0]);
        lmt = GET_AV_INT(limit->child[1]);
    }

    if (with_header) {
        qrl = realloc(qrl, (++(*row_count)) * sizeof(QueryResult*));
        qrl[(*row_count) - 1] = get_select_header(t, expect_cols);
    }

    while ((qr = filter_row(t, c, expect_cols, where_top_list)) != NULL) {
        if (offset-- > 0) {
            destory_query_result(qr, *col_count);
            continue;
        }

        qrl = realloc(qrl, ++(*row_count) * sizeof(QueryResult*));
        qrl[(*row_count) - 1] = qr;

        if (--lmt == 0) {
            break;
        }
    }

    cursor_destory(c);
    return qrl;
}

static inline void remove_row_from_table(Table* t, Page* p, int dir_num)
{
    set_row_deleted(p, dir_num);
    incr_table_row_cnt(t, -1);
}

int delete_row(DB* d, Ast* delete_ast)
{
    assert(delete_ast->kind == AST_DELETE);

    Ast *table_name, *where_top_list;
    Table* t;
    Cursor* c;
    int deleted_row_count = 0;

    table_name = delete_ast->child[0];
    where_top_list = delete_ast->child[1];
    t = open_table(d, GET_AV_STR(table_name->child[0]));

    if (t == NULL) {
        printf("table `%s` not exits.\n", GET_AV_STR(table_name->child[0]));
        return 0;
    }

    if (where_top_list != NULL && !check_where_ast_valid(t, where_top_list->child[0])) {
        return 0;
    }

    c = cursor_init(t);

    while (!cursor_is_end(c)) {
        if (!cursor_value_is_deleted(c) && get_where_expr_res(t, cursor_page(c), cursor_dir_num(c), where_top_list)) {
            remove_row_from_table(t, c->page, c->page_dir_num);
            deleted_row_count++;
        }
        cursor_next(c);
    }

    cursor_destory(c);
    return deleted_row_count;
}

static int check_set_list_valid(Table* t, Ast* set_list)
{
    assert(set_list->kind == AST_UPDATE_SET_LIST);

    for (int i = 0; i < set_list->children; i++) {
        if (get_col_num_by_col_name(t->row_fmt, GET_AV_STR(set_list->child[i]->child[0])) == -1) {
            printf("col `%s` not exist.\n", GET_AV_STR(set_list->child[i]->child[0]));
            return 0;
        }
    }

    return 1;
}

static int update_row(Table* t, Page* p, int dir_num, Ast* set_list)
{
    assert(set_list->kind == AST_UPDATE_SET_LIST);

    // 获取原来的数据
    QueryResult* qr;
    QueryResultVal* origin;
    Ast *col_name, *expr;
    int row_cnt, col_num, res;

    qr = get_table_row(t, p, dir_num, &row_cnt);

    for (int i = 0; i < set_list->children; i++) {
        col_name = set_list->child[i]->child[0];
        expr = set_list->child[i]->child[1];

        col_num = get_col_num_by_col_name(t->row_fmt, GET_AV_STR(col_name));
        origin = qr[col_num];
        qr[col_num] = get_expr_res(t, p, dir_num, expr);
        destory_query_result_val(origin);
    }
    // 替换原有数据
    res = replace_row(t, p, dir_num, t->row_fmt, qr);
    destory_query_result(qr, row_cnt);
    return res;
}

int update_table(DB* d, Ast* update)
{
    assert(update->kind == AST_UPDATE);
    Ast *set_list, *table_name, *where, *limit;
    Table* t;
    Cursor* c;
    int updated_row_count = 0;
    int lmt = -1;
    int offset = 0;

    table_name = update->child[0];
    set_list = update->child[1];
    where = update->child[2];
    limit = update->child[3];
    t = open_table(d, GET_AV_STR(table_name->child[0]));

    if (t == NULL) {
        printf("table `%s` not exits.\n", GET_AV_STR(table_name->child[0]));
        return 0;
    }

    if (where != NULL && !check_where_ast_valid(t, where->child[0])) {
        return 0;
    }

    if (!check_set_list_valid(t, set_list)) {
        return 0;
    }

    if (limit != NULL) {
        offset = GET_AV_INT(limit->child[0]);
        lmt = GET_AV_INT(limit->child[1]);
    }

    c = cursor_init(t);

    while (!cursor_is_end(c)) {
        if (!cursor_value_is_deleted(c) && get_where_expr_res(t, cursor_page(c), cursor_dir_num(c), where)) {
            if (offset-- <= 0) {
                updated_row_count += update_row(t, cursor_page(c), cursor_dir_num(c), set_list);

                if (--lmt == 0) {
                    cursor_next(c);
                    break;
                }
            }
        }
        cursor_next(c);
    }

    cursor_destory(c);
    return updated_row_count;
}

// | len | table_count | table_fmt | ... |
DB* db_init()
{
    DB* d;
    int fd, dat_len, table_count, rec_len;
    void *data, *origin;
    Table* t;
    d = smalloc(sizeof(DB));
    d->tables = ht_init(NULL, NULL);

    // if (access("db.dat", 0) == 0) {
    //     fd = open("db.dat", O_RDWR, 0644);
    //     read(fd, &dat_len, sizeof(int)); // len
    //     data = smalloc(dat_len);
    //     origin = data;
    //     read(fd, data, dat_len - sizeof(int));
    //     memcpy(&table_count, data, sizeof(int));
    //     data += sizeof(int);

    //     // for (int i = 0; i < table_count; i++) {
    //     //     t = unserialize_table(data, &rec_len);
    //     //     ht_insert(d->tables, t->name, t);
    //     //     data += rec_len;
    //     // }

    //     free(origin);
    //     close(fd);
    // }

    return d;
}

static void free_row_fmt(RowFmt* rf)
{
    for (int i = 0; i < rf->origin_cols_count; i++) {
        free(rf->origin_cols_name[i]);
    }

    for (int i = 0; i < rf->static_cols_count; i++) {
        free(rf->static_cols_name[i]);
    }

    for (int i = 0; i < rf->dynamic_cols_count; i++) {
        free(rf->dynamic_cols_name[i]);
    }

    free(rf->cols_fmt);
    free(rf->origin_cols_name);
    free(rf->dynamic_cols_name);
    free(rf->static_cols_name);
    free(rf);
}

static void parse_fmt_list(Ast* a, RowFmt* rf)
{
    assert(a->kind == AST_COL_FMT_LIST);

    Ast* col_fmt;

    rf->origin_cols_name = smalloc(sizeof(char*) * a->children);
    rf->origin_cols_count = a->children;
    rf->cols_fmt = smalloc(sizeof(ColFmt) * a->children);
    rf->dynamic_cols_count = 0;
    rf->static_cols_count = 0;
    rf->dynamic_cols_name = NULL;
    rf->static_cols_name = NULL;

    for (int i = 0; i < a->children; i++) {
        col_fmt = a->child[i];

        if (col_fmt->attr == C_INT || col_fmt->attr == C_CHAR || col_fmt->attr == C_DOUBLE) {
            rf->static_cols_name = realloc(rf->static_cols_name, sizeof(char*) * (++rf->static_cols_count));
            rf->static_cols_name[rf->static_cols_count - 1] = strdup(GET_AV_STR(col_fmt->child[0]));
            rf->cols_fmt[i].is_dynamic = 0;
        } else if (col_fmt->attr == C_VARCHAR) {
            rf->dynamic_cols_name = realloc(rf->dynamic_cols_name, sizeof(char*) * (++rf->dynamic_cols_count));
            rf->dynamic_cols_name[rf->dynamic_cols_count - 1] = strdup(GET_AV_STR(col_fmt->child[0]));
            rf->cols_fmt[i].is_dynamic = 1;
        }

        if (col_fmt->attr == C_DOUBLE) {
            rf->cols_fmt[i].len = sizeof(double);
        } else if (col_fmt->attr == C_INT) {
            rf->cols_fmt[i].len = sizeof(int);
        } else {
            rf->cols_fmt[i].len = GET_AV_INT(col_fmt->child[1]);
        }

        rf->origin_cols_name[i] = strdup(GET_AV_STR(col_fmt->child[0]));
        rf->cols_fmt[i].type = col_fmt->attr;
    }
}

static QueryResultList* execute_select_sql(DB* d, char* sql, int* row_cnt, int* col_cnt)
{
    QueryResultList* qrl;
    Ast* origin;
    origin = G_AST;
    lex_read(sql, strlen(sql));
    assert(G_AST->kind == AST_SELECT);

    qrl = select_row(d, G_AST, row_cnt, col_cnt, 0);
    ast_destory(G_AST);
    G_AST = origin;
    return qrl;
}

static int execute_insert_sql(DB* d, char* sql)
{
    int res;
    Ast* origin;
    origin = G_AST;
    lex_read(sql, strlen(sql));
    assert(G_AST->kind == AST_INSERT);

    res = insert_row(d, G_AST);
    ast_destory(G_AST);
    G_AST = origin;
    return res;
}

static int execute_update_sql(DB* d, char* sql)
{
    int res;
    Ast* origin;
    origin = G_AST;
    lex_read(sql, strlen(sql));
    assert(G_AST->kind == AST_UPDATE);

    res = update_table(d, G_AST);
    ast_destory(G_AST);
    G_AST = origin;
    return res;
}

int create_table(DB* d, Ast* a)
{
    // assert(a->kind == AST_CREATE);

    // Table* t;
    // Ast *table_name, *fmt_list;
    // char file_name[1024] = { 0 };
    // int row_cnt, col_cnt;

    // table_name = a->child[0]->child[0];
    // fmt_list = a->child[1];

    // // 将格式插入sys_cols中，表名插入sys_tables

    // sprintf(sql, "select * from sys_tables where table_name = %s;", GET_AV_STR(table_name));

    // if (ht_find(d->tables, GET_AV_STR(table_name)) != NULL) {
    //     execute_select_sql(d, sql, &row_cnt, &col_cnt);
    //     printf("table already exist.\n");
    //     return -1;
    // }

    // t = smalloc(sizeof(Table));
    // t->name = strdup(GET_AV_STR(table_name));

    // get_table_file_name(t->name, file_name);
    // t->data_fd = open(file_name, O_CREAT | O_RDWR, 0644);
    // t->pager = smalloc(sizeof(Pager));

    // for (int i = 0; i < MAX_PAGE_CNT_P_TABLE; i++) {
    //     t->pager->pages[i] = NULL;
    //     t->free_map[i] = PAGE_SIZE - sizeof(t->pager->pages[i]->header);
    // }

    // t->max_page_num = 0;
    // t->row_fmt = smalloc(sizeof(RowFmt));
    // get_page(t, 0);
    // parse_fmt_list(fmt_list, t->row_fmt);
    // ht_insert(d->tables, GET_AV_STR(table_name), t);
    // return 0;
}

/**
 * sys_tables: name max_page_num row_count
 * sys_cols: table_name col_name origin_col_num col_type len 
 * sys_free_space table_name page_num free
 */
Table* open_table(DB* d, char* name)
{
    char sql[1024] = { 0 };
    int row_cnt, col_cnt;
    QueryResultList* qrl;
    Table* t = NULL;
    char table_file_name[1024];

    get_table_file_name(name, table_file_name);

    if ((t = ht_find(d->tables, name)) == NULL) {
        if (strcmp(name, SYS_TABLES) == 0 || strcmp(name, SYS_COLS) == 0) {
            if (access(table_file_name, 0) != 0) {
                // 初始化sys_tables
                get_table_file_name(SYS_TABLES, table_file_name);
                t = smalloc(sizeof(Table));
                t->db = d;
                t->name = strdup(SYS_TABLES);
                t->data_fd = open(table_file_name, O_CREAT | O_RDWR, 0644);
                init_sys_table_pager(t);
                t->row_fmt = smalloc(sizeof(RowFmt));
                init_sys_tables_row_fmt(t->row_fmt);
                ht_insert(d->tables, SYS_TABLES, t);
                // 插入表名
                sprintf(sql, "insert into %s values('%s', 0, 0);", SYS_TABLES, SYS_TABLES);
                execute_insert_sql(d, sql);
                sprintf(sql, "insert into %s values('%s', 0, 0);", SYS_TABLES, SYS_COLS);
                execute_insert_sql(d, sql);

                // 初始化sys_cols表
                get_table_file_name(SYS_COLS, table_file_name);
                t = smalloc(sizeof(Table));
                t->db = d;
                t->name = strdup(SYS_COLS);
                t->data_fd = open(table_file_name, O_CREAT | O_RDWR, 0644);
                init_sys_table_pager(t);
                t->row_fmt = smalloc(sizeof(RowFmt));
                init_sys_clos_row_fmt(t->row_fmt);
                ht_insert(d->tables, SYS_COLS, t);
                // 插入表结构
                sprintf(sql, "insert into %s values('%s', 'name', 0, %d, 255),('%s', 'max_page_num', 1, %d, 11),('%s', 'row_count', 2, %d, 11);", SYS_COLS, SYS_TABLES, C_CHAR, SYS_TABLES, C_INT, SYS_TABLES, C_INT);
                execute_insert_sql(d, sql);
                sprintf(sql, "insert into %s values('%s','table_name',0,%d,255),('%s','col_name',1,%d,255),('%s','origin_col_num',2,%d,11),('%s','col_type',3,%d,11),('%s','len',4,%d,11);", SYS_COLS, SYS_COLS, C_CHAR, SYS_COLS, C_CHAR, SYS_COLS, C_INT, SYS_COLS, C_INT, SYS_COLS, C_INT);
                execute_insert_sql(d, sql);

                // free space插入SYS_FREE_SPACE
                // for (int i = 0; i < MAX_PAGE_CNT_P_TABLE; i++) {
                //     sprintf(sql, "insert into %s values('%s',%d,%ld);", SYS_FREE_SPACE, name, i, PAGE_SIZE - sizeof(Page));
                //     execute_insert_sql(d, sql);
                // }

                return ht_find(d->tables, name);
            }
        } else if (strcmp(name, SYS_FREE_SPACE) == 0) {
            if (access(table_file_name, 0) != 0) {
                // t = smalloc(sizeof(Table));
                // t->db = d;
                // t->name = strdup(name);
                // // 初始化sys_free_space表, sys_free_space table_name page_num free
                // t->data_fd = open(table_file_name, O_CREAT | O_RDWR, 0644);
                // init_sys_table_pager(t);
                // t->row_fmt = smalloc(sizeof(RowFmt));

                // init_sys_free_space_row_fmt(t->row_fmt);
                // ht_insert(d->tables, name, t);
                // // 表名插入sys_tables
                // sprintf(sql, "insert into %s values('%s', 0, 0);", SYS_TABLES, name);
                // execute_insert_sql(d, sql);
                // // 结构插入cols
                // sprintf(sql, "insert into %s values('%s','table_name',0,%d,255),('%s','page_num',1,%d,11),('%s','free',2,%d,11);", SYS_COLS, name, C_CHAR, name, C_INT, name, C_INT);
                // execute_insert_sql(d, sql);
                // free space插入SYS_FREE_SPACE
                // for (int i = 0; i < MAX_PAGE_CNT_P_TABLE; i++) {
                //     sprintf(sql, "insert into %s values('%s',%d,%ld);", SYS_FREE_SPACE, name, i, PAGE_SIZE - sizeof(Page));
                //     execute_insert_sql(d, sql);
                // }
            }
        } else {
            // sprintf(sql, "select * from %s;", SYS_TABLES);
            // qrl = execute_select_sql(d, sql, &row_cnt, &col_cnt);

            // if (qrl == NULL) {
            //     return NULL;
            // }
            // t = smalloc(sizeof(Table));
            // t->db = d;
            // t->name = strdup(name);
            // t->data_fd = open(table_file_name, O_CREAT | O_RDWR, 0644);
            // destory_query_result_list(qrl, row_cnt, col_cnt);

            // // 获取表的结构信息
            // // sprintf(sql, "select * from %s where table_name = '' order by origin_col_num asc;", SYS_TABLES);
            // sprintf(sql, "select * from %s where table_name = '';", SYS_TABLES);
            // qrl = execute_select_sql(d, sql, &row_cnt, &col_cnt);

            // t->row_fmt = scalloc(sizeof(RowFmt), 1);

            // assert(qrl != NULL);
            // t->row_fmt->origin_cols_count = row_cnt;
            // t->row_fmt->origin_cols_name = smalloc(sizeof(char*) * row_cnt);
            // t->row_fmt->cols_fmt = scalloc(sizeof(ColFmt) * row_cnt, 1);

            // ColType ct;
            // // table_name col_name origin_col_num col_type len
            // for (int i = 0; i < row_cnt; i++) {
            //     memcpy(&ct, qrl[i][3]->data, sizeof(int));

            //     if (ct == C_VARCHAR) {
            //         t->row_fmt->dynamic_cols_name = realloc(t->row_fmt->dynamic_cols_name, sizeof(char*) * ++t->row_fmt->dynamic_cols_count);
            //         t->row_fmt->dynamic_cols_name[t->row_fmt->dynamic_cols_count - 1] = strdup(qrl[i][1]->data);
            //     } else {
            //         t->row_fmt->static_cols_name = realloc(t->row_fmt->static_cols_name, sizeof(char*) * ++t->row_fmt->static_cols_count);
            //         t->row_fmt->static_cols_name[t->row_fmt->static_cols_count - 1] = strdup(qrl[i][1]->data);
            //     }

            //     t->row_fmt->origin_cols_name[t->row_fmt->origin_cols_count++] = strdup(qrl[i][1]->data);
            //     t->row_fmt->cols_fmt[i].is_dynamic = ct == C_VARCHAR;
            //     memcpy(&t->row_fmt->cols_fmt[i].len, qrl[i][4]->data, sizeof(int));
            //     memcpy(&t->row_fmt->cols_fmt[i].type, qrl[i][3]->data, sizeof(int));
            // }

            // destory_query_result_list(qrl, row_cnt, col_cnt);
            // // 获取空闲page信息 table_name page_num free
            // sprintf(sql, "select * from %s where table_name = '%s';", SYS_FREE_SPACE, name);
            // qrl = execute_select_sql(d, sql, &row_cnt, &col_cnt);
            // t->pager = smalloc(sizeof(Pager));

            // for (int i = 0; i < MAX_PAGE_CNT_P_TABLE; i++) {
            //     t->pager->pages[i] = NULL;
            //     // t->free_map[i] = PAGE_SIZE - sizeof(Page);
            //     memcpy(&t->free_map[i], qrl[i][2], sizeof(int));
            // }
            // destory_query_result_list(qrl, row_cnt, col_cnt);
        }
    }

    return t;
}

void table_flush(Table* t)
{
    for (int i = 0; i <= get_table_max_page_num(t); i++) {
        flush_page(t, i);
    }
}

void table_destory(Table* t)
{
    free(t->name);
    close(t->data_fd);

    for (int i = 0; i < MAX_PAGE_CNT_P_TABLE; i++) {
        if (t->pager->pages[i] != NULL) {
            free(t->pager->pages[i]);
        }
    }

    free(t->pager);
    free_row_fmt(t->row_fmt);
    free(t);
}

void db_destory(DB* d)
{
    FOREACH_HT(d->tables, k, t)
    table_flush(t);
    ENDFOREACH()

    FOREACH_HT(d->tables, k, t)
    table_destory(t);
    ENDFOREACH()

    ht_release(d->tables);
    free(d);
}