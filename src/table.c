#include "include/table.h"
#include "include/util.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int get_where_expr_res(Table* t, int page_num, int dir_num, Ast* where_expr);
static QueryResultVal* get_expr_res(Table* t, int page_num, int dir_num, Ast* expr);
static QueryResultVal* av_to_qrv(Ast* a);
static int execute_update_sql(DB* d, char* sql);
static int execute_insert_sql(DB* d, char* sql);
static int execute_delete_sql(DB* d, char* sql);
static QueryResultList* execute_select_sql(DB* d, char* sql, int* row_cnt, int* col_cnt);
int lex_read(char* s, int len);
static int qrv_to_int(QueryResultVal* qrv);
static void init_all_sys_tables(DB* d);
static void init_all_store_tables(DB* d);
static void table_destory(Table* t);
static int delete_table(DB* d, Table* t);

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

// sys_tables: char(255) name int(4) row_count
static void init_sys_tables_row_fmt(RowFmt* rf)
{
    rf->origin_cols_name = smalloc(sizeof(char*) * 2);
    rf->origin_cols_name[0] = strdup("name");
    rf->origin_cols_name[1] = strdup("row_count");
    rf->origin_cols_count = 2;
    rf->static_cols_name = smalloc(sizeof(char*) * 2);
    rf->static_cols_name[0] = strdup("name");
    rf->static_cols_name[1] = strdup("row_count");
    rf->static_cols_count = 2;
    rf->dynamic_cols_count = 0;
    rf->dynamic_cols_name = NULL;
    rf->cols_fmt = smalloc(sizeof(ColFmt) * 2);
    rf->cols_fmt[0].is_dynamic = 0;
    rf->cols_fmt[0].type = C_CHAR;
    rf->cols_fmt[0].len = 255;
    rf->cols_fmt[1].is_dynamic = 0;
    rf->cols_fmt[1].type = C_INT;
    rf->cols_fmt[1].len = 4;
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
    rf->cols_fmt[1].len = 4;
    rf->cols_fmt[2].is_dynamic = 0;
    rf->cols_fmt[2].type = C_INT;
    rf->cols_fmt[2].len = 4;
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
    rf->cols_fmt[2].len = 4;
    rf->cols_fmt[3].is_dynamic = 0;
    rf->cols_fmt[3].type = C_INT;
    rf->cols_fmt[3].len = 4;
    rf->cols_fmt[4].is_dynamic = 0;
    rf->cols_fmt[4].type = C_INT;
    rf->cols_fmt[4].len = 4;
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

inline int get_table_row_cnt(Table* t)
{
    int row_cnt, col_cnt, res;
    char sql[1024];
    QueryResultList* qrl;

    sprintf(sql, "select row_count from %s where name = '%s';", SYS_TABLES, t->name);
    qrl = execute_select_sql(t->db, sql, &row_cnt, &col_cnt);
    assert(qrl != NULL);

    res = qrv_to_int(qrl[0][0]);
    destory_query_result_list(qrl, row_cnt, col_cnt);
    return res;
}

int get_page_free_space_stored(Table* t, int page_num)
{
    int row_cnt, col_cnt, res;
    char sql[1024];
    QueryResultList* qrl;

    sprintf(sql, "select free from %s where table_name = '%s' and page_num = %d;", SYS_FREE_SPACE, t->name, page_num);
    qrl = execute_select_sql(t->db, sql, &row_cnt, &col_cnt);
    assert(qrl != NULL);

    res = qrv_to_int(qrl[0][0]);
    destory_query_result_list(qrl, row_cnt, col_cnt);
    return res;
}

int insert_table_free_space(Table* t, int page_num, int free_space)
{
    char sql[1024];
    int size, new_free_space, res, len;
    void* data;
    QueryResult* qr;

    // 如果是sys_space_free table, 直接插入, 不要调用execute_insert_sql, 否则再次会触发reserve_new_row_space, 引发bug
    // 拿到当前页, 计算后的大小直接插入
    if (strcmp(t->name, SYS_FREE_SPACE) == 0) {
        qr = smalloc(sizeof(QueryResultVal*) * 3);
        qr[0] = smalloc(sizeof(QueryResultVal));
        qr[0]->type = C_CHAR;
        qr[0]->data = strdup(SYS_FREE_SPACE);

        qr[1] = smalloc(sizeof(QueryResultVal));
        qr[1]->type = C_INT;
        qr[1]->data = smalloc(sizeof(int));
        memcpy(qr[1]->data, &page_num, sizeof(int));

        qr[2] = smalloc(sizeof(QueryResultVal));
        qr[2]->type = C_INT;
        qr[2]->data = smalloc(sizeof(int));
        size = calc_serialized_row_len(t->row_fmt, qr);
        new_free_space = free_space - (get_sizeof_dir() + size);
        memcpy(qr[2]->data, &new_free_space, sizeof(int));

        data = serialize_row(t->row_fmt, qr, &len);
        res = write_row_head(t, page_num, data, len);
        free(data);
        incr_table_row_cnt(t, 1);
        destory_query_result(qr, 3);
    } else {
        sprintf(sql, "insert into %s values( '%s', %d, %d);", SYS_FREE_SPACE, t->name, page_num, free_space);
        res = execute_insert_sql(t->db, sql);
    }

    return res;
}

int incr_table_free_space(Table* t, int page_num, int step)
{
    char sql[1024];
    if (step >= 0) {
        sprintf(sql, "update %s set free = free + %d where table_name = '%s' and page_num = %d;", SYS_FREE_SPACE, step, t->name, page_num);
    } else {
        sprintf(sql, "update %s set free = free - %d where table_name = '%s' and page_num = %d;", SYS_FREE_SPACE, -step, t->name, page_num);
    }
    return execute_update_sql(t->db, sql);
}

static int add_col_fmt(DB* d, char* table_name, char* col_name, int origin_col_num, ColType col_type, int len)
{
    char sql[1024];
    sprintf(sql, "insert into %s values('%s', '%s', %d, %d, %d);", SYS_COLS, table_name, col_name, origin_col_num, col_type, len);
    return execute_insert_sql(d, sql);
}

static int add_table(DB* d, char* table_name)
{
    char sql[1024];
    sprintf(sql, "insert into %s values('%s', 0);", SYS_TABLES, table_name);
    return execute_insert_sql(d, sql);
}

static int add_row_to_table(Table* t, QueryResult* qr)
{
    int res, len;
    void* data;
    data = serialize_row(t->row_fmt, qr, &len);
    res = write_row_to_page(t, data, len);
    free(data);
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

static QueryResultVal* cmd_bool(Table* t, int page_num, int dir_num, Ast* op1, Ast* op2, ExprOp op)
{
    QueryResultVal *op1_qrv, *op2_qrv, *res_qrv;

    op1_qrv = get_expr_res(t, page_num, dir_num, op1);

    if (op != E_NOT) {
        op2_qrv = get_expr_res(t, page_num, dir_num, op2);
        res_qrv = do_bool(qrv_to_bool(op1_qrv), qrv_to_bool(op2_qrv), op);
        destory_query_result_val(op1_qrv);
        destory_query_result_val(op2_qrv);
    } else {
        res_qrv = do_bool(qrv_to_bool(op1_qrv), 0, op);
        destory_query_result_val(op1_qrv);
    }

    return res_qrv;
}

static QueryResultVal* cmd_cacl(Table* t, int page_num, int dir_num, Ast* op1, Ast* op2, ExprOp op)
{
    QueryResultVal *op1_qrv, *op2_qrv, *res_qrv;

    op1_qrv = get_expr_res(t, page_num, dir_num, op1);

    if (op == E_B_NOT) {
        destory_query_result_val(op1_qrv);
        return do_calc_int(qrv_to_int(op1_qrv), 0, op);
    }

    op2_qrv = get_expr_res(t, page_num, dir_num, op2);

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
static QueryResultVal* cmd_compare(Table* t, int page_num, int dir_num, Ast* op1, Ast* op2, ExprOp op)
{
    QueryResultVal *op1_qrv, *op2_qrv, *res_qrv;

    op1_qrv = get_expr_res(t, page_num, dir_num, op1);
    op2_qrv = get_expr_res(t, page_num, dir_num, op2);

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

static QueryResultVal* get_expr_res(Table* t, int page_num, int dir_num, Ast* expr)
{
    assert(expr->kind == AST_EXP || expr->kind == AST_VAL);

    Ast *op1, *op2;
    ExprOp op;
    QueryResultVal* qrv;
    void* data;

    op = expr->attr;

    if (expr->kind == AST_VAL) {
        if (GET_AV_TYPE(expr) == AVT_ID) {
            data = copy_row_raw_data(t, page_num, dir_num);
            qrv = unserialize_row(data, t->row_fmt, GET_AV_STR(expr));
            free(data);
        } else {
            qrv = av_to_qrv(expr);
        }

        return qrv;
    }

    op1 = expr->child[0];
    op2 = expr->child[1];

    if (op == E_EQ || op == E_NEQ || op == E_GT || op == E_GTE || op == E_LT || op == E_LTE) {
        return cmd_compare(t, page_num, dir_num, op1, op2, op);
    } else if (op == E_NOT || op == E_OR || op == E_AND) {
        return cmd_bool(t, page_num, dir_num, op1, op2, op);
    }

    return cmd_cacl(t, page_num, dir_num, op1, op2, op);
}

static int get_where_expr_res(Table* t, int page_num, int dir_num, Ast* where_expr)
{
    assert(where_expr == NULL || where_expr->kind == AST_WHERE_EXP);

    int res;
    QueryResultVal* res_qrv;

    if (where_expr == NULL) {
        return 1;
    }

    res_qrv = get_expr_res(t, page_num, dir_num, where_expr->child[0]);
    res = qrv_to_bool(res_qrv);
    destory_query_result_val(res_qrv);
    return res;
}

QueryResult* filter_row(Table* t, Cursor* c, Ast* expect_cols, Ast* where_top_expr)
{
    QueryResult* qr;
    QueryResultVal* qrv;
    QueryResult* full_row;
    int col_cnt = 0;
    int table_row_count = 0;
    void* data;

    while (!cursor_is_end(c)) {
        if (!cursor_value_is_deleted(c) && get_where_expr_res(t, cursor_page_num(c), cursor_dir_num(c), where_top_expr)) {
            qr = smalloc(expect_cols->children * sizeof(QueryResultVal*));
            // 符合条件,返回这条数据
            for (int i = 0; i < expect_cols->children; i++) {
                if (GET_AV_TYPE(expect_cols->child[i]) != AVT_ID) {
                    qr[col_cnt++] = av_to_qrv(expect_cols->child[i]);
                } else {
                    if (strcmp(GET_AV_STR(expect_cols->child[i]), "*") == 0) {
                        data = copy_row_raw_data(t, cursor_page_num(c), cursor_dir_num(c));
                        full_row = unserialize_full_row(data, t->row_fmt, &table_row_count);
                        free(data);

                        for (int i = 0; i < table_row_count; i++) {
                            qr = realloc(qr, ++col_cnt * sizeof(QueryResultVal*));
                            qr[col_cnt - 1] = full_row[i];
                        }

                        free(full_row);
                    } else {
                        data = copy_row_raw_data(t, cursor_page_num(c), cursor_dir_num(c));
                        qrv = unserialize_row(data, t->row_fmt, GET_AV_STR(expect_cols->child[i]));
                        free(data);
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

static Table* copy_tmp_table(DB* d, Table* origin_table)
{
    char tmp_name[1024];

    sprintf(tmp_name, "%s#$~tmp", origin_table->name);

    for (int i = 0; i < origin_table->row_fmt->origin_cols_count; i++) {
        add_col_fmt(d, tmp_name, origin_table->row_fmt->origin_cols_name[i], i, origin_table->row_fmt->cols_fmt[i].type, origin_table->row_fmt->cols_fmt[i].len);
    }

    add_table(d, tmp_name);
    return open_table(d, tmp_name);
}

int row_cmp_func(const void* p1, const void* p2)
{
    QueryResult *qr1, *qr2;
    QueryResultVal *qrv1, *qrv2;

    qr1 = (QueryResult*)p1;
    qr2 = (QueryResult*)p2;
    qrv1 = qr1[0];
    qrv2 = qr2[0];

    if (qrv1->type == C_INT) {
        return qrv_to_int(qrv1) - qrv_to_int(qrv2) < 0 ? 1 : -1;
    } else if (qrv1->type == C_DOUBLE) {
        return qrv_to_double(qrv1) - qrv_to_double(qrv2) < 0 ? 1 : -1;
    } else {
        return strcmp(qrv1->data, qrv2->data) < 0 ? 1 : -1;
    }
}

// TODO，生成临时表
static Table* make_order_tmp_table(DB* d, Table* origin_table, Ast* order_by)
{
    assert(order_by->kind == AST_ORDER_BY);

    Table* tmp_table;
    int row_cnt, col_cnt;
    QueryResultList* qrl;
    char sql[1024];

    // 将原表所有数据拿出
    tmp_table = copy_tmp_table(d, origin_table);
    sprintf(sql, "select * from %s;", origin_table->name);
    qrl = execute_select_sql(d, sql, &row_cnt, &col_cnt);

    if (qrl == NULL) {
        return tmp_table;
    }

    qsort(qrl, row_cnt, sizeof(QueryResult*), row_cmp_func);

    for (int i = 0; i < row_cnt; i++) {
        add_row_to_table(tmp_table, qrl[i]);
    }

    destory_query_result_list(qrl, row_cnt, col_cnt);
    return tmp_table;
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

    if (order_by != NULL) {
        t = make_order_tmp_table(d, t, order_by); // 下面直接用临时表查询
        cursor_destory(c);
        c = cursor_init(t);
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

    if (order_by != NULL) {
        // 删除临时表
        delete_table(d, t);
    }

    cursor_destory(c);
    return qrl;
}

static inline void remove_row_from_table(Table* t, int page_num, int dir_num)
{
    set_row_deleted(t, page_num, dir_num);
    incr_table_row_cnt(t, -1);
}

static int delete_table(DB* d, Table* t)
{
    char sql[1024];
    char file_name[1024];

    sprintf(sql, "delete from %s where `name` = '%s';", SYS_TABLES, t->name);
    execute_delete_sql(d, sql);
    sprintf(sql, "delete from %s where `table_name` = '%s';", SYS_COLS, t->name);
    execute_delete_sql(d, sql);
    sprintf(sql, "delete from %s where `table_name` = '%s';", SYS_FREE_SPACE, t->name);
    execute_delete_sql(d, sql);
    get_table_file_name(t->name, file_name);
    unlink(file_name);
    ht_delete(d->tables, t->name);
    table_destory(t);
    return 0;
}

int drop_table(DB* d, Ast* drop_table_ast)
{
    assert(drop_table_ast->kind == AST_DROP_TABLE);

    Ast* table_name;
    Table* t;

    table_name = drop_table_ast->child[0];
    t = open_table(d, GET_AV_STR(table_name->child[0]));
    return delete_table(d, t);
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
        if (!cursor_value_is_deleted(c) && get_where_expr_res(t, cursor_page_num(c), cursor_dir_num(c), where_top_list)) {
            remove_row_from_table(t, cursor_page_num(c), cursor_dir_num(c));
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

static int update_row(Table* t, int page_num, int dir_num, Ast* set_list)
{
    assert(set_list->kind == AST_UPDATE_SET_LIST);

    // 获取原来的数据
    QueryResult* qr;
    QueryResultVal* origin;
    Ast *col_name, *expr;
    int row_cnt, col_num, res, len;
    void* data;

    data = copy_row_raw_data(t, page_num, dir_num);
    qr = unserialize_full_row(data, t->row_fmt, &row_cnt);
    free(data);

    for (int i = 0; i < set_list->children; i++) {
        col_name = set_list->child[i]->child[0];
        expr = set_list->child[i]->child[1];

        col_num = get_col_num_by_col_name(t->row_fmt, GET_AV_STR(col_name));
        origin = qr[col_num];
        qr[col_num] = get_expr_res(t, page_num, dir_num, expr);
        destory_query_result_val(origin);
    }

    data = serialize_row(t->row_fmt, qr, &len);
    // 替换原有数据
    res = replace_row(t, page_num, dir_num, data, len);
    destory_query_result(qr, row_cnt);
    free(data);
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
        if (!cursor_value_is_deleted(c) && get_where_expr_res(t, cursor_page_num(c), cursor_dir_num(c), where)) {
            if (offset-- <= 0) {
                updated_row_count += update_row(t, cursor_page_num(c), cursor_dir_num(c), set_list);
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

DB* db_init()
{
    DB* d;
    d = smalloc(sizeof(DB));
    d->tables = ht_init(NULL, NULL);
    init_GP(10);

    if (access("t_sys_tables.dat", 0) == 0) {
        init_all_store_tables(d);
    } else {
        init_all_sys_tables(d);
    }

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

static int execute_delete_sql(DB* d, char* sql)
{
    int res;
    Ast* origin;
    origin = G_AST;
    lex_read(sql, strlen(sql));
    assert(G_AST->kind == AST_DELETE);

    res = delete_row(d, G_AST);
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
    assert(a->kind == AST_CREATE);

    Ast *table_name, *fmt_list, *col_fmt;
    int len;

    table_name = a->child[0]->child[0];
    fmt_list = a->child[1];

    if (open_table(d, GET_AV_STR(table_name)) != NULL) {
        printf("table `%s` exist\n", GET_AV_STR(table_name));
        return -1;
    }

    for (int i = 0; i < fmt_list->children; i++) {
        col_fmt = fmt_list->child[i];
        if (col_fmt->attr == C_DOUBLE) {
            len = sizeof(double);
        } else if (col_fmt->attr == C_INT) {
            len = sizeof(int);
        } else {
            len = GET_AV_INT(col_fmt->child[1]);
        }
        add_col_fmt(d, GET_AV_STR(table_name), GET_AV_STR(col_fmt->child[0]), i, col_fmt->attr, len);
    }

    add_table(d, GET_AV_STR(table_name));
    return 0;
}

void init_sys_tables(DB* d)
{
    Table* t;
    char table_file_name[1024];
    get_table_file_name(SYS_TABLES, table_file_name);
    t = smalloc(sizeof(Table));
    t->db = d;
    t->name = strdup(SYS_TABLES);
    t->row_fmt = smalloc(sizeof(RowFmt));
    init_sys_tables_row_fmt(t->row_fmt);
    t->pager = init_pager(open(table_file_name, O_CREAT | O_RDWR, 0644));
    ht_insert(d->tables, SYS_TABLES, t);
}

void init_sys_cols(DB* d)
{
    Table* t;
    char table_file_name[1024];
    get_table_file_name(SYS_COLS, table_file_name);
    t = smalloc(sizeof(Table));
    t->db = d;
    t->name = strdup(SYS_COLS);
    t->row_fmt = smalloc(sizeof(RowFmt));
    init_sys_clos_row_fmt(t->row_fmt);
    t->pager = init_pager(open(table_file_name, O_CREAT | O_RDWR, 0644));
    ht_insert(d->tables, SYS_COLS, t);
}

void init_sys_free_space(DB* d)
{
    Table* t;
    char table_file_name[1024];
    get_table_file_name(SYS_FREE_SPACE, table_file_name);
    t = smalloc(sizeof(Table));
    t->db = d;
    t->name = strdup(SYS_FREE_SPACE);
    t->row_fmt = smalloc(sizeof(RowFmt));
    init_sys_free_space_row_fmt(t->row_fmt);
    t->pager = init_pager(open(table_file_name, O_CREAT | O_RDWR, 0644));
    ht_insert(d->tables, SYS_FREE_SPACE, t);
}

void init_all_store_tables(DB* d)
{
    // 初始化sys_free_space表
    init_sys_free_space(d);
    // 初始化sys_tables
    init_sys_tables(d);
    // 初始化cols
    init_sys_cols(d);
}

void init_all_sys_tables(DB* d)
{
    // Table* t;
    char sql[1024] = { 0 };
    // 初始化sys_free_space表
    init_sys_free_space(d);
    // 初始化sys_tables
    init_sys_tables(d);
    // 初始化sys_cols表
    init_sys_cols(d);
    // 插入表名
    sprintf(sql, "insert into %s values('%s', 0);", SYS_TABLES, SYS_TABLES);
    execute_insert_sql(d, sql);
    sprintf(sql, "insert into %s values('%s', 2);", SYS_TABLES, SYS_FREE_SPACE);
    execute_insert_sql(d, sql);
    sprintf(sql, "insert into %s values('%s', 0);", SYS_TABLES, SYS_COLS);
    execute_insert_sql(d, sql);

    // 插入表结构
    sprintf(sql, "insert into %s values('%s', 'name', 0, %d, 255),('%s', 'row_count', 2, %d, 4);", SYS_COLS, SYS_TABLES, C_CHAR, SYS_TABLES, C_INT);
    execute_insert_sql(d, sql);
    sprintf(sql, "insert into %s values('%s','table_name',0,%d,255),('%s','col_name',1,%d,255),('%s','origin_col_num',2,%d,4),('%s','col_type',3,%d,4),('%s','len',4,%d,4);", SYS_COLS, SYS_COLS, C_CHAR, SYS_COLS, C_CHAR, SYS_COLS, C_INT, SYS_COLS, C_INT, SYS_COLS, C_INT);
    execute_insert_sql(d, sql);
    sprintf(sql, "insert into %s values('%s','table_name',0,%d,255),('%s','page_num',1,%d,4),('%s','free',2,%d,4);", SYS_COLS, SYS_FREE_SPACE, C_CHAR, SYS_FREE_SPACE, C_INT, SYS_FREE_SPACE, C_INT);
    execute_insert_sql(d, sql);
}

/**
 * sys_tables: name max_page_num row_count
 * sys_cols: table_name col_name origin_col_num col_type len 
 * sys_free_space table_name page_num free
 */
Table* open_table(DB* d, char* name)
{
    Table* t;
    char table_file_name[1024];
    char sql[1024];
    QueryResultList* qrl;
    int row_cnt, col_cnt;

    if ((t = ht_find(d->tables, name)) == NULL) {
        sprintf(sql, "select * from %s where name = '%s';", SYS_TABLES, name);
        qrl = execute_select_sql(d, sql, &row_cnt, &col_cnt);

        if (qrl == NULL) {
            return NULL;
        }

        destory_query_result_list(qrl, row_cnt, col_cnt);

        t = smalloc(sizeof(Table));
        t->db = d;
        t->name = strdup(name);
        get_table_file_name(name, table_file_name);
        t->pager = init_pager(open(table_file_name, O_CREAT | O_RDWR, 0644));
        t->row_fmt = smalloc(sizeof(RowFmt));

        // sprintf(sql, "select * from %s where table_name = '%s' order by origin_col_num;", SYS_COLS, name);
        sprintf(sql, "select * from %s where table_name = '%s';", SYS_COLS, name);
        qrl = execute_select_sql(d, sql, &row_cnt, &col_cnt);

        t->row_fmt->origin_cols_count = row_cnt;
        t->row_fmt->origin_cols_name = smalloc(sizeof(char*) * row_cnt);
        t->row_fmt->static_cols_name = NULL;
        t->row_fmt->static_cols_count = 0;
        t->row_fmt->dynamic_cols_name = NULL;
        t->row_fmt->dynamic_cols_count = 0;
        t->row_fmt->cols_fmt = smalloc(sizeof(ColFmt) * row_cnt);

        for (int i = 0; i < row_cnt; i++) {
            t->row_fmt->origin_cols_name[i] = strdup(qrl[i][1]->data);
            t->row_fmt->cols_fmt[i].type = qrv_to_int(qrl[i][3]);
            t->row_fmt->cols_fmt[i].len = qrv_to_int(qrl[i][4]);
            t->row_fmt->cols_fmt[i].is_dynamic = t->row_fmt->cols_fmt[i].type == C_VARCHAR;

            if (t->row_fmt->cols_fmt[i].is_dynamic) {
                t->row_fmt->dynamic_cols_name = realloc(t->row_fmt->dynamic_cols_name, sizeof(char*) * ++t->row_fmt->dynamic_cols_count);
                t->row_fmt->dynamic_cols_name[t->row_fmt->dynamic_cols_count - 1] = strdup(qrl[i][1]->data);
            } else {
                t->row_fmt->static_cols_name = realloc(t->row_fmt->static_cols_name, sizeof(char*) * ++t->row_fmt->static_cols_count);
                t->row_fmt->static_cols_name[t->row_fmt->static_cols_count - 1] = strdup(qrl[i][1]->data);
            }
        }
        ht_insert(d->tables, name, t);
        destory_query_result_list(qrl, row_cnt, col_cnt);
    }

    return t;
}

void table_flush(Table* t)
{
    flush_pager_header(t->pager);
}

static void table_destory(Table* t)
{
    free(t->name);
    close(t->pager->data_fd);

    free(t->pager);
    free_row_fmt(t->row_fmt);
    free(t);
}

void db_destory(DB* d)
{
    destory_GP();

    FOREACH_HT(d->tables, k, t)
    table_flush(t);
    ENDFOREACH()

    FOREACH_HT(d->tables, k, t)
    table_destory(t);
    ENDFOREACH()

    ht_release(d->tables);
    free(d);
}