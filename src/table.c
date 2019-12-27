#include "include/table.h"
#include "include/util.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static inline void copy_ast_val(void* dest, AstVal* a)
{
    int i;
    double d;

    if (GET_AV_TYPE(a) == AVT_INT) {
        i = GET_AV_INT(a);
        memcpy(dest, &i, GET_AV_LEN(a));
    } else if (GET_AV_TYPE(a) == AVT_DOUBLE) {
        d = GET_AV_DOUBLE(a);
        memcpy(dest, &d, GET_AV_LEN(a));
    } else {
        memcpy(dest, GET_AV_STR(a), GET_AV_LEN(a));
    }
}

inline int get_query_result_val_len(QueryResultVal* qrv)
{
    if (qrv->type == C_INT) {
        return sizeof(int);
    } else if (qrv->type == C_DOUBLE) {
        return sizeof(double);
    } else if (qrv->type == C_CHAR || qrv->type == C_VARCHAR) {
        return strlen(qrv->data);
    }

    return 0;
}

static void get_table_file_name(char* name, char* file_name)
{
    sprintf(file_name, "t_%s.dat", name);
}

Table* unserialize_table(void* serialized, int* len)
{
    int rec_len = 0;
    int col_fmt_len = 0;
    int col_name_size = 0;
    void* origin;
    Table* t;

    origin = serialized;
    t = smalloc(sizeof(Table));
    t->row_fmt = smalloc(sizeof(RowFmt));

    memcpy(&rec_len, serialized, sizeof(int)); // rec_len
    serialized += sizeof(int);
    memcpy(&(t->max_page_num), serialized, sizeof(int)); // max_page_num
    serialized += sizeof(int);
    memcpy(&(t->row_count), serialized, sizeof(int)); // row_count
    serialized += sizeof(int);
    memcpy(&(t->row_fmt->origin_cols_count), serialized, sizeof(int)); // col_count
    serialized += sizeof(int);

    // 初始化结构
    t->row_fmt->origin_cols_name = smalloc(sizeof(char*) * t->row_fmt->origin_cols_count);
    t->row_fmt->cols_fmt = smalloc(sizeof(ColFmt) * t->row_fmt->origin_cols_count);
    t->row_fmt->dynamic_cols_count = 0;
    t->row_fmt->dynamic_cols_name = NULL;
    t->row_fmt->static_cols_count = 0;
    t->row_fmt->static_cols_name = NULL;

    for (int i = 0; i < t->row_fmt->origin_cols_count; i++) {
        memcpy(&col_fmt_len, serialized, sizeof(int)); // col_fmt_len
        serialized += sizeof(int);
        memcpy(&(t->row_fmt->cols_fmt[i].type), serialized, sizeof(int)); // col_type
        serialized += sizeof(int);
        memcpy(&(t->row_fmt->cols_fmt[i].len), serialized, sizeof(int)); // col_len
        serialized += sizeof(int);
        col_name_size = col_fmt_len - 3 * sizeof(int);
        t->row_fmt->origin_cols_name[i] = strndup(serialized, col_name_size); // col_name

        if (t->row_fmt->cols_fmt[i].type == C_INT || t->row_fmt->cols_fmt[i].type == C_DOUBLE || t->row_fmt->cols_fmt[i].type == C_CHAR) {
            t->row_fmt->static_cols_count++;
            t->row_fmt->static_cols_name = realloc(t->row_fmt->static_cols_name, t->row_fmt->static_cols_count * sizeof(char*));
            t->row_fmt->static_cols_name[t->row_fmt->static_cols_count - 1] = strndup(serialized, col_name_size);
            t->row_fmt->cols_fmt[i].is_dynamic = 0;
        } else if (t->row_fmt->cols_fmt[i].type == C_VARCHAR) {
            t->row_fmt->dynamic_cols_count++;
            t->row_fmt->dynamic_cols_name = realloc(t->row_fmt->dynamic_cols_name, t->row_fmt->dynamic_cols_count * sizeof(char*));
            t->row_fmt->dynamic_cols_name[t->row_fmt->dynamic_cols_count - 1] = strndup(serialized, col_name_size);
            t->row_fmt->cols_fmt[i].is_dynamic = 1;
        }

        serialized += col_name_size; // skip rec
    }

    t->pager = smalloc(sizeof(Pager));

    for (int i = 0; i < MAX_PAGE_CNT_P_TABLE; i++) {
        t->pager->pages[i] = NULL;
        memcpy(&(t->free_map[i]), serialized, sizeof(int));
        serialized += sizeof(int);
    }

    t->name = smalloc(origin + rec_len - serialized);
    memcpy(t->name, serialized, origin + rec_len - serialized);

    if (len != NULL) {
        *len = rec_len;
    }

    char file_name[255];
    get_table_file_name(t->name, file_name);
    t->data_fd = open(file_name, O_RDWR, 0644);
    return t;
}

/**
 *  | len | max_page_num | row_count | col_count | col_fmt_len | col_type | col_len | col_name | ... | free_map | ... | table_name
 */
void* serialize_table(Table* t, int* len)
{
    int rec_len = 0;
    int col_fmt_len = 0;
    int col_name_size = 0;
    void *rec, *origin;

    rec_len += 4 * sizeof(int); // len max_page_num row_count col_count

    // 计算字段格式所占长度
    for (int i = 0; i < t->row_fmt->origin_cols_count; i++) {
        rec_len += 3 * sizeof(int); // col_fmt_len col_type col_len
        rec_len += strlen(t->row_fmt->origin_cols_name[i]) + 1; // with '\0'
    }

    rec_len += MAX_PAGE_CNT_P_TABLE * sizeof(int); // freemap
    rec_len += strlen(t->name) + 1; // with '\0'

    rec = scalloc(rec_len, 1);
    origin = rec;
    memcpy(rec, &rec_len, sizeof(int)); // len
    rec += sizeof(int);
    memcpy(rec, &t->max_page_num, sizeof(int)); // max_page_num
    rec += sizeof(int);
    memcpy(rec, &t->row_count, sizeof(int)); // row_count
    rec += sizeof(int);
    memcpy(rec, &t->row_fmt->origin_cols_count, sizeof(int)); // row_count
    rec += sizeof(int);

    for (int i = 0; i < t->row_fmt->origin_cols_count; i++) {
        col_name_size = strlen(t->row_fmt->origin_cols_name[i]) + 1;
        col_fmt_len = 3 * sizeof(int) + col_name_size;
        memcpy(rec, &col_fmt_len, sizeof(int)); // col_fmt_len
        rec += sizeof(int);
        memcpy(rec, &(t->row_fmt->cols_fmt[i].type), sizeof(int)); // col_type
        rec += sizeof(int);
        memcpy(rec, &(t->row_fmt->cols_fmt[i].len), sizeof(int)); // col_len
        rec += sizeof(int);
        memcpy(rec, t->row_fmt->origin_cols_name[i], col_name_size); // col_name
        rec += col_name_size;
    }

    for (int i = 0; i < MAX_PAGE_CNT_P_TABLE; i++) {
        memcpy(rec, &(t->free_map[i]), sizeof(int));
        rec += sizeof(int);
    }

    memcpy(rec, t->name, strlen(t->name) + 1);

    if (len != NULL) {
        *len = rec_len;
    }

    return origin;
}

void destory_query_result_val(QueryResultVal* qrv)
{
    free(qrv->data);
    free(qrv);
}

static int add_row_to_table(Table* t, QueryResult* qr)
{
    serialize_row(reserve_row_space(t, cacl_serialized_row_len(t->row_fmt, qr)), t->row_fmt, qr);
    t->row_count++;
    return 0;
}

static QueryResultVal* parse_insert_row_val(ColFmt* cf, Ast* a)
{
    AstVal* av;
    assert(a->kind == AST_VAL);
    int i;
    double d;

    av = (AstVal*)a;
    QueryResultVal* qrv;
    qrv = smalloc(sizeof(QueryResultVal));
    qrv->type = cf->type;

    if (GET_AV_TYPE(a) == AVT_STR) {
        if (cf->type == C_CHAR || cf->type == C_VARCHAR) {
            qrv->data = strdup(GET_AV_STR(a));
        } else if (cf->type == C_INT) {
            i = atoi(GET_AV_STR(a));
            qrv->data = smalloc(sizeof(int));
            memcpy(qrv->data, &i, sizeof(int));
        } else {
            // double
            d = atoi(GET_AV_STR(a));
            qrv->data = smalloc(sizeof(double));
            memcpy(qrv->data, &d, sizeof(double));
        }
    } else if (GET_AV_TYPE(a) == AVT_INT) {
        if (cf->type == C_CHAR || cf->type == C_VARCHAR) {
            qrv->data = strdup(""); // TODO
        } else if (cf->type == C_INT) {
            i = GET_AV_INT(a);
            qrv->data = smalloc(sizeof(int));
            memcpy(qrv->data, &i, sizeof(int));
        } else {
            // double
            qrv->data = smalloc(sizeof(double));
            d = (double)GET_AV_INT(a);
            memcpy(qrv->data, &d, sizeof(double));
        }
    } else if (GET_AV_TYPE(a) == AVT_DOUBLE) {
        if (cf->type == C_CHAR || cf->type == C_VARCHAR) {
            qrv->data = strdup(""); // TODO
        } else if (cf->type == C_INT) {
            qrv->data = smalloc(sizeof(int));
            i = (int)GET_AV_DOUBLE(a);
            memcpy(qrv->data, &i, sizeof(int));
        } else {
            // double
            qrv->data = smalloc(sizeof(double));
            d = GET_AV_DOUBLE(a);
            memcpy(qrv->data, &d, sizeof(double));
        }
    } else {
        return NULL;
    }

    return qrv;
}

static QueryResult* parse_insert_row(RowFmt* rf, Ast* a)
{
    assert(a->kind == AST_INSERT_VAL_LIST);

    QueryResult* qr;

    qr = smalloc(sizeof(QueryResultVal*) * rf->origin_cols_count);

    for (int i = 0; i < rf->origin_cols_count; i++) {
        qr[i] = parse_insert_row_val(&rf->cols_fmt[i], a->child[i]);
    }

    return qr;
}

/**
 *  cvs必须跟列定义时的顺序一致, 不支持null
 */
int insert_row(DB* d, Ast* a)
{
    assert(a->kind == AST_INSERT);

    Table* t;
    Ast *table_name, *row_list, *val_list;
    QueryResult* qr;

    table_name = a->child[0];
    row_list = a->child[1];

    if ((t = open_table(d, GET_AV_STR(table_name->child[0]))) == NULL) {
        printf("table %s not exist\n", GET_AV_STR(table_name->child[0]));
        return -1;
    }

    for (int i = 0; i < row_list->children; i++) {
        val_list = row_list->child[i];

        if (val_list->children != t->row_fmt->origin_cols_count) {
            printf("expect %d cols, but given %d rows", t->row_fmt->origin_cols_count, val_list->children);
            return -1;
        }

        qr = parse_insert_row(t->row_fmt, val_list);
        add_row_to_table(t, qr);
        destory_query_result(qr, t->row_fmt->origin_cols_count);
    }

    return 0;
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

QueryResultVal* get_real_val_by_op(Table* t, void* row, Ast* op)
{
    QueryResultVal* qrv;
    int where_res = 0;

    assert(op->kind == AST_VAL || op->kind == AST_WHERE_EXP);

    if (op->kind == AST_VAL) {
        if (GET_AV_TYPE(op) == AVT_ID) {
            qrv = get_col_val(row, t->row_fmt, GET_AV_STR(op));
        } else if (GET_AV_TYPE(op) == AVT_DOUBLE) {
            qrv = smalloc(sizeof(QueryResultVal));
            qrv->type = C_DOUBLE;
            qrv->data = smalloc(sizeof(double));
            copy_ast_val(qrv->data, (AstVal*)op);
        } else if (GET_AV_TYPE(op) == AVT_INT) {
            qrv = smalloc(sizeof(QueryResultVal));
            qrv->type = C_INT;
            qrv->data = smalloc(sizeof(int));
            copy_ast_val(qrv->data, (AstVal*)op);
        } else {
            // AVT_STR
            qrv = smalloc(sizeof(QueryResultVal));
            qrv->type = C_CHAR;
            qrv->data = smalloc(GET_AV_LEN(op));
            copy_ast_val(qrv->data, (AstVal*)op);
        }
    } else if (op->kind == AST_WHERE_EXP) {
        qrv = smalloc(sizeof(QueryResultVal));
        qrv->type = C_INT;
        qrv->data = smalloc(sizeof(int));
        where_res = get_where_expr_res(t, row, op);
        memcpy(qrv->data, &where_res, sizeof(int));
    } else {
        return NULL;
    }

    return qrv;
}

static int do_compare_int(int op1, int op2, ExprOp op)
{
    if (op == E_EQ) {
        return op1 == op2;
    } else if (op == E_GT) {
        return op1 > op2;
    } else if (op == E_GTE) {
        return op1 >= op2;
    } else if (op == E_LT) {
        return op1 < op2;
    } else if (op == E_LTE) {
        return op1 <= op2;
    } else if (op == E_NEQ) {
        return op1 != op2;
    }

    return 0;
}

static int do_compare_double(double op1, double op2, ExprOp op)
{
    if (op == E_EQ) {
        return op1 == op2;
    } else if (op == E_GT) {
        return op1 > op2;
    } else if (op == E_GTE) {
        return op1 >= op2;
    } else if (op == E_LT) {
        return op1 < op2;
    } else if (op == E_LTE) {
        return op1 <= op2;
    } else if (op == E_NEQ) {
        return op1 != op2;
    }

    return 0;
}

static int do_compare_str(char* op1, char* op2, ExprOp op)
{
    int res;
    res = strcmp(op1, op2);
    if (op == E_EQ) {
        return res == 0;
    } else if (op == E_GT) {
        return res > 0;
    } else if (op == E_GTE) {
        return res >= 0;
    } else if (op == E_LT) {
        return res < 0;
    } else if (op == E_LTE) {
        return res <= 0;
    } else if (op == E_NEQ) {
        return res != 0;
    }

    return 0;
}

int do_bool_op(int op1, int op2, ExprOp op)
{
    if (op == E_AND) {
        return op1 && op2;
    } else if (op == E_OR) {
        return op1 || op2;
    } else if (op == E_NOT) {
        return !op1;
    }

    return 0;
}

int cmd_bool(Table* t, void* row, Ast* op1, Ast* op2, ExprOp op)
{
    QueryResultVal *op1_qrv, *op2_qrv;
    int res;

    op1_qrv = get_real_val_by_op(t, row, op1);

    if (op != E_NOT) {
        op2_qrv = get_real_val_by_op(t, row, op2);
        res = do_bool_op(qrv_to_bool(op1_qrv), qrv_to_bool(op2_qrv), op);
        destory_query_result_val(op1_qrv);
        destory_query_result_val(op2_qrv);
    } else {
        res = do_bool_op(qrv_to_bool(op1_qrv), 0, op);
        destory_query_result_val(op1_qrv);
    }

    return res;
}

// 如果一个是字符串，一个是数字，那么都转成数字比较
int cmd_compare(Table* t, void* row, Ast* op1, Ast* op2, ExprOp op)
{
    int res, i1, i2;
    double d1, d2;
    QueryResultVal *op1_qrv, *op2_qrv;

    op1_qrv = get_real_val_by_op(t, row, op1);
    op2_qrv = get_real_val_by_op(t, row, op2);

    if (op1_qrv->type == C_INT) {
        memcpy(&i1, op1_qrv->data, sizeof(int));
        if (op2_qrv->type == C_INT) {
            memcpy(&i2, op2_qrv->data, sizeof(int));
            res = do_compare_int(i1, i2, op);
        } else if (op2_qrv->type == C_DOUBLE) {
            memcpy(&d2, op2_qrv->data, sizeof(double));
            res = do_compare_double((double)i1, d2, op);
        } else {
            i2 = atoi(op2_qrv->data);
            res = do_compare_int(i1, i2, op);
        }
    } else if (op1_qrv->type == C_DOUBLE) {
        memcpy(&d1, op1_qrv->data, sizeof(double));
        if (op2_qrv->type == C_INT) {
            memcpy(&i2, op2_qrv->data, sizeof(int));
            res = do_compare_double(d1, (double)i2, op);
        } else if (op2_qrv->type == C_DOUBLE) {
            memcpy(&d2, op2_qrv->data, sizeof(double));
            res = do_compare_double(d1, d2, op);
        } else {
            d2 = atof(op2_qrv->data);
            res = do_compare_double(d1, d2, op);
        }
    } else {
        // char varchar
        if (op2_qrv->type == C_INT) {
            memcpy(&i2, op2_qrv->data, sizeof(int));
            res = do_compare_int(atoi(op1_qrv->data), i2, op);
        } else if (op2_qrv->type == C_DOUBLE) {
            memcpy(&d2, op2_qrv->data, sizeof(double));
            res = do_compare_double(atof(op1_qrv->data), d2, op);
        } else {
            res = do_compare_str(op1_qrv->data, op2_qrv->data, op);
        }
    }

    destory_query_result_val(op1_qrv);
    destory_query_result_val(op2_qrv);
    return res;
}

inline int get_table_row_cnt(Table* t)
{
    return t->row_count;
}

static int get_where_expr_res(Table* t, void* row, Ast* where_expr)
{
    assert(where_expr->kind == AST_WHERE_EXP || where_expr->kind == AST_VAL);

    Ast *op1, *op2;
    ExprOp op;
    QueryResultVal* qrv;
    int res = 0;

    if (where_expr->kind == AST_VAL) {
        qrv = get_real_val_by_op(t, row, where_expr);
        res = qrv_to_bool(qrv);
        destory_query_result_val(qrv);
        return res;
    }

    op1 = where_expr->child[0];
    op2 = where_expr->child[1];
    op = where_expr->attr;

    if (op == E_EQ || op == E_NEQ || op == E_GT || op == E_GTE || op == E_LT || op == E_LTE) {
        return cmd_compare(t, row, op1, op2, op);
    }

    if (op == E_NOT || op == E_OR || op == E_AND) {
        return cmd_bool(t, row, op1, op2, op);
    }

    return 0;
}

QueryResult* filter_row(Table* t, Cursor* c, Ast* expect_cols, Ast* where_top_expr)
{
    QueryResult* qr;
    QueryResultVal* qrv;
    int col_cnt = 0;

    while (!cursor_is_end(c)) {
        if (where_top_expr == NULL || get_where_expr_res(t, cursor_value(c), where_top_expr->child[0]) == 1) {
            qr = smalloc(expect_cols->children * sizeof(QueryResultVal*));
            // 符合条件,返回这条数据
            for (int i = 0; i < expect_cols->children; i++) {
                if (GET_AV_TYPE(expect_cols->child[i]) == AVT_STR) {
                    qrv = smalloc(sizeof(QueryResultVal));
                    // 直接返回这个字符串
                    qrv->type = C_CHAR;
                    qrv->data = strdup(GET_AV_STR(expect_cols->child[i]));
                    qr[col_cnt++] = qrv;
                } else if (GET_AV_TYPE(expect_cols->child[i]) == AVT_DOUBLE) {
                    qrv = smalloc(sizeof(QueryResultVal));
                    qrv->type = C_DOUBLE;
                    qrv->data = smalloc(sizeof(double));
                    copy_ast_val(qrv->data, (AstVal*)(expect_cols->child[i]));
                    qr[col_cnt++] = qrv;
                } else if (GET_AV_TYPE(expect_cols->child[i]) == AVT_INT) {
                    qrv = smalloc(sizeof(QueryResultVal));
                    qrv->type = C_INT;
                    qrv->data = smalloc(sizeof(int));
                    copy_ast_val(qrv->data, (AstVal*)(expect_cols->child[i]));
                    qr[col_cnt++] = qrv;
                } else {
                    if (strcmp(GET_AV_STR(expect_cols->child[i]), "*") == 0) {
                        for (int j = 0; j < t->row_fmt->origin_cols_count; j++) {
                            col_cnt++;
                            qr = realloc(qr, col_cnt * sizeof(QueryResultVal*));
                            qr[col_cnt - 1] = get_col_val(cursor_value(c), t->row_fmt, t->row_fmt->origin_cols_name[j]);
                        }
                    } else {
                        qrv = get_col_val(cursor_value(c), t->row_fmt, GET_AV_STR(expect_cols->child[i]));
                        qr[col_cnt++] = qrv;
                    }
                }
            }

            // 移动指针
            cursor_next(c, 1);
            return qr;
        } else {
            // 继续下一条
            cursor_next(c, 1);
        }
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
                printf("col %s not exist\n", GET_AV_STR(expect_cols->child[i]));
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
                count++;
                qr = realloc(qr, sizeof(QueryResultVal*) * count);
                qr[count - 1] = smalloc(sizeof(QueryResultVal));
                qr[count - 1]->data = strdup(t->row_fmt->origin_cols_name[j]);
                qr[count - 1]->type = C_CHAR;
            }
        } else {
            count++;
            qr = realloc(qr, sizeof(QueryResultVal*) * count);
            qr[count - 1] = smalloc(sizeof(QueryResultVal));
            qr[count - 1]->data = strdup(GET_AV_STR(expect_cols->child[i]));
            qr[count - 1]->type = C_CHAR;
        }
    }

    return qr;
}

QueryResultList* select_row(DB* d, Ast* select_ast, int* row_count, int* col_count)
{
    assert(select_ast->kind == AST_SELECT);

    Ast *table_name, *expect_cols, *where_top_list;
    Table* t;
    QueryResultList* qrl = NULL;
    QueryResult* qr;
    Cursor* c;

    expect_cols = select_ast->child[0];
    table_name = select_ast->child[1];
    where_top_list = select_ast->child[2];
    t = open_table(d, GET_AV_STR(table_name->child[0]));

    if (t == NULL) {
        printf("table `%s` not exits\n", GET_AV_STR(table_name->child[0]));
        return NULL;
    }

    c = cursor_init(t);
    (*row_count) = 0;
    *col_count = check_and_get_select_col_count(t, expect_cols);

    if (*col_count == -1) {
        *row_count = 0;
        return NULL;
    }

    (*row_count)++;
    qrl = realloc(qrl, ((*row_count)) * sizeof(QueryResult*));
    qrl[(*row_count) - 1] = get_select_header(t, expect_cols);

    while ((qr = filter_row(t, c, expect_cols, where_top_list)) != NULL) {
        (*row_count)++;
        qrl = realloc(qrl, (*row_count) * sizeof(QueryResult*));
        qrl[(*row_count) - 1] = qr;
    }

    cursor_destory(c);
    return qrl;
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

    if (access("db.dat", 0) == 0) {
        fd = open("db.dat", O_RDWR, 0644);
        read(fd, &dat_len, sizeof(int)); // len
        data = smalloc(dat_len);
        origin = data;
        read(fd, data, dat_len - sizeof(int));
        memcpy(&table_count, data, sizeof(int));
        data += sizeof(int);

        for (int i = 0; i < table_count; i++) {
            t = unserialize_table(data, &rec_len);
            ht_insert(d->tables, t->name, t);
            data += rec_len;
        }

        free(origin);
        close(fd);
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

int create_table(DB* d, Ast* a)
{
    assert(a->kind == AST_CREATE);

    Table* t;
    Ast *table_name, *fmt_list;
    char file_name[1024] = { 0 };

    table_name = a->child[0]->child[0];
    fmt_list = a->child[1];
    t = smalloc(sizeof(Table));
    t->name = strdup(GET_AV_STR(table_name));

    if (ht_find(d->tables, t->name) != NULL) {
        printf("table exist\n");
        return -1;
    }

    get_table_file_name(t->name, file_name);
    t->data_fd = open(file_name, O_CREAT | O_RDWR, 0644);
    t->pager = smalloc(sizeof(Pager));

    for (int i = 0; i < MAX_PAGE_CNT_P_TABLE; i++) {
        t->pager->pages[i] = NULL;
        t->free_map[i] = PAGE_SIZE - sizeof(t->pager->pages[i]->header);
    }

    t->max_page_num = 0;
    t->row_count = 0;
    t->row_fmt = smalloc(sizeof(RowFmt));
    get_page(t, 0);
    parse_fmt_list(fmt_list, t->row_fmt);
    ht_insert(d->tables, GET_AV_STR(table_name), t);
    return 0;
}

Table* open_table(DB* d, char* name)
{
    return (Table*)ht_find(d->tables, name);
}

void table_destory(Table* t)
{
    for (int i = 0; i <= t->max_page_num; i++) {
        flush_page(t, i);
    }

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
    int fd, len, total_len;
    void* data;
    total_len = 0;

    fd = open("db.dat", O_RDWR | O_CREAT, 0644);
    total_len += 2 * sizeof(int);

    lseek(fd, sizeof(int), SEEK_SET);
    write(fd, &(d->tables->cnt), sizeof(int));

    FOREACH_HT(d->tables, k, t)
    data = serialize_table(t, &len);
    table_destory(t);
    write(fd, data, len);
    total_len += len;
    free(data);
    ENDFOREACH()

    lseek(fd, 0, SEEK_SET);
    write(fd, &(total_len), sizeof(int));
    close(fd);
    ht_release(d->tables);
    free(d);
}