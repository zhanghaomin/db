#include "include/table.h"
#include "include/util.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Cursor* cursor_init(Table* t)
{
    Cursor* c;
    c = scalloc(sizeof(Cursor), 1);
    c->table = t;
    c->pager = t->pager;
    cursor_rewind(c);
    return c;
}

void cursor_destory(Cursor* c)
{
    free(c);
}

void* cursor_value(Cursor* c)
{
    return (void*)(c->page->data + c->offset);
}

int cursor_is_end(Cursor* c)
{
    return c->row_num == c->table->row_count;
}

static int get_row_len(void* row_start)
{
    RowHeader rh;
    memcpy(&rh, row_start, sizeof(rh));
    return rh.row_len;
}

Page* get_page(Table* t, int page_num)
{
    Page* new_page;

    if (t->pager->pages[page_num] == NULL) {
        new_page = scalloc(PAGE_SIZE, 1);
        t->pager->pages[page_num] = new_page;

        if (page_num > t->max_page_num) { // 新空间,不需要读磁盘
            new_page->header.page_num = page_num;
            new_page->header.row_count = 0;
            new_page->header.tail = 0;
            t->max_page_num = page_num;
        } else { // 旧数据
            read(t->data_fd, new_page, PAGE_SIZE);
        }
    }

    return t->pager->pages[page_num];
}

int flush_page(Table* t, int page_num)
{
    if (t->pager->pages[page_num] == NULL) {
        return 0;
    }

    if (lseek(t->data_fd, PAGE_SIZE * page_num, SEEK_SET) == -1) {
        return -1;
    }

    if (write(t->data_fd, t->pager->pages[page_num], PAGE_SIZE) == -1) {
        return -1;
    }

    return 0;
}

static int cursor_reach_page_end(Cursor* c)
{
    return c->page_row_num == c->page->header.row_count;
}

void cursor_next(Cursor* c)
{
    if (cursor_is_end(c)) {
        return;
    }

    c->offset += get_row_len(cursor_value(c));
    c->page_row_num++;

    if (cursor_reach_page_end(c)) {
        c->page = get_page(c->table, c->page->header.page_num + 1);
        c->offset = 0;
        c->page_row_num = 0;
    }

    c->row_num++;
    return;
}

void cursor_rewind(Cursor* c)
{
    c->page = get_page(c->table, 0);
    c->offset = 0;
    c->page_row_num = 0;
    c->row_num = 0;
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

void println_rows(Table* t, QueryResultList* qrl, int qrl_len, int qr_len)
{
    int* paddings;
    paddings = scalloc(sizeof(int), qr_len);
    char s[1024];

    for (int i = 0; i < t->row_fmt->origin_cols_count; i++) {
        if (paddings[i] < (int)strlen(t->row_fmt->origin_cols_name[i])) {
            paddings[i] = (int)strlen(t->row_fmt->origin_cols_name[i]);
        }
    }

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
    println_table_col_name(t, paddings);
    println_row_line(paddings, qr_len);
    for (int i = 0; i < qrl_len; i++) {
        println_row(qrl[i], qr_len, paddings);
        println_row_line(paddings, qr_len);
    }
    free(paddings);
}

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

void traverse_table(Cursor* c)
{
    QueryResultList* qrl;
    int qr_len;

    cursor_rewind(c);
    qr_len = c->table->row_fmt->origin_cols_count;
    qrl = NULL;

    int i = 0;

    while (!cursor_is_end(c)) {
        qrl = realloc(qrl, (i + 1) * sizeof(QueryResult*));
        qrl[i] = scalloc(qr_len * sizeof(QueryResultVal*), 1);

        for (int j = 0; j < qr_len; j++) {
            qrl[i][j] = smalloc(sizeof(QueryResultVal));
        }

        unserialize_row(cursor_value(c), c->table->row_fmt, qrl[i]);
        cursor_next(c);
        i++;
    }

    println_rows(c->table, qrl, i, qr_len);
    destory_query_result_list(qrl, i, qr_len);
}

Page* find_free_page(Table* t, int size)
{
    Page* find_page;
    int i = 0;

    for (; i <= MAX_PAGE_CNT_P_TABLE; i++) {
        if (size <= t->free_map[i]) {
            find_page = get_page(t, i);
            return find_page;
        }
    }

    // never happen
    printf("page no space\n");
    exit(-1);
}

static void* get_page_tail(Page* p)
{
    return (void*)(p->data + p->header.tail);
}

ColFmt* get_col_fmt_by_col_name(RowFmt* rf, char* col_name)
{
    for (int i = 0; i < rf->static_cols_count; i++) {
        if (strcmp(rf->static_cols_name[i], col_name) == 0) {
            return &rf->cols_fmt[i];
        }
    }

    for (int i = 0; i < rf->dynamic_cols_count; i++) {
        if (strcmp(rf->dynamic_cols_name[i], col_name) == 0) {
            return &rf->cols_fmt[rf->static_cols_count + i];
        }
    }

    return NULL;
}

int get_static_col_num_by_col_name(RowFmt* rf, char* col_name)
{
    for (int i = 0; i < rf->static_cols_count; i++) {
        if (strcmp(rf->static_cols_name[i], col_name) == 0) {
            return i;
        }
    }

    return -1;
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

int cacl_serialized_row_len(RowFmt* rf, Ast* val_list)
{
    assert(val_list->kind == AST_INSERT_VAL_LIST);

    int result = 0;
    ColFmt* cf;

    result += sizeof(RowHeader);

    for (int i = 0; i < val_list->children; i++) {
        cf = &rf->cols_fmt[i];

        if (cf->is_dynamic) { // 动态属性需要额外空间储存偏移量
            result += sizeof(int);
            result += GET_AV_LEN(val_list->child[i]);
        } else { // 静态属性定长
            result += cf->len;
        }

        result += sizeof(int);
    }

    return result;
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

/*
    || RowHeader || 1st dynamic offset || 2nd dynamic offset || ... || 1st reclen | 1st static rec || 2nd reclen | 2nd static rec || ... || 1st dynamic reclen | 1st dynamic rec || 2nd dynamic reclen | 2nd dynamic rec || ...
                            |______________________________________________________________________________________________________________|
                                                  |________________________________________________________________________________________________________________________________|
     偏移量都是相对header的
*/
// 4 + 4 + 4 + 4 + 8 + 4 + 255 + 4 + 52
int serialize_row(void* store, RowFmt* rf, Ast* val_list)
{
    assert(val_list->kind == AST_INSERT_VAL_LIST);

    ColFmt* cf;
    RowHeader rh;
    AstVal* val;
    rh.row_len = 0;
    void *dynamic_offset_store, *data_start;
    int offset = 0;
    int val_len = 0;

    data_start = store + sizeof(int);
    dynamic_offset_store = data_start;
    store += sizeof(int) + sizeof(int) * rf->dynamic_cols_count;

    // 写入静态字段
    for (int i = 0; i < val_list->children; i++) {
        cf = &rf->cols_fmt[i];

        if (!cf->is_dynamic) {
            val = (AstVal*)(val_list->child[i]);
            val_len = GET_AV_LEN(val);
            memcpy(store, &val_len, sizeof(int));
            store += sizeof(int);
            copy_ast_val(store, val);
            // important: 静态字段固定长度
            store += cf->len;
            rh.row_len += sizeof(int);
            rh.row_len += cf->len;
        }
    }

    // 写入动态字段, 修改偏移量
    for (int i = 0; i < val_list->children; i++) {
        cf = &rf->cols_fmt[i];

        if (cf->is_dynamic) {
            val = (AstVal*)(val_list->child[i]);
            val_len = GET_AV_LEN(val);
            offset = store - data_start;
            memcpy(dynamic_offset_store, &offset, sizeof(int));
            dynamic_offset_store += sizeof(int);
            memcpy(store, &val_len, sizeof(int));
            store += sizeof(int);
            copy_ast_val(store, val);
            store += val_len;
            rh.row_len += 2 * sizeof(int);
            rh.row_len += val_len;
        }
    }

    // 长度加上自己
    rh.row_len += sizeof(RowHeader);
    // 写入header
    memcpy(data_start - sizeof(RowHeader), &rh, sizeof(RowHeader));
    return 0;
}

QueryResultVal* get_col_val(void* row, RowFmt* rf, char* col_name)
{
    int col_num, dynamic_col_num, static_col_num, len;
    void* col;
    ColFmt cf;
    QueryResultVal* qrv;
    qrv = smalloc(sizeof(QueryResultVal));
    col_num = get_col_num_by_col_name(rf, col_name);
    row += sizeof(RowHeader);

    if (col_num == -1) {
        return NULL;
    }

    cf = rf->cols_fmt[col_num];
    qrv->type = cf.type;

    if (cf.is_dynamic) {
        dynamic_col_num = get_dynamic_col_num_by_col_name(rf, col_name);
        col = row + sizeof(int) * dynamic_col_num;
    } else {
        static_col_num = get_static_col_num_by_col_name(rf, col_name);
        col = row + sizeof(int) * rf->dynamic_cols_count + static_col_num * sizeof(int);
    }

    memcpy(&len, col, sizeof(int)); // len
    col += sizeof(int);
    qrv->data = smalloc(len);
    memcpy(qrv->data, col, len);
    return qrv;
}

/*
 * 返回反序列化好的字段数组, 顺序和定义时的一致
*/
void unserialize_row(void* row, RowFmt* rf, QueryResult* qr)
{
    void* header;
    int offset, col_val_len, col_num;

    // skip header
    row += sizeof(RowHeader);
    header = row;

    // 4 + 4 + 4 + 8 + 4 + 255 + 4 + 52
    // copy dynamic col
    for (int i = 0; i < rf->dynamic_cols_count; i++) {
        // 获取动态字段偏移量
        memcpy(&offset, row, sizeof(int));
        // 获取字段长度
        memcpy(&col_val_len, header + offset, sizeof(int));
        // 获取当前字段在所有字段的序号(按定义时顺序)
        col_num = get_col_num_by_col_name(rf, rf->dynamic_cols_name[i]);
        qr[col_num]->type = rf->cols_fmt[col_num].type;
        qr[col_num]->data = smalloc(col_val_len);
        // 复制字段内容
        memcpy(qr[col_num]->data, header + offset + sizeof(int), col_val_len);
        // next offset
        row += sizeof(int);
    }

    // skip dynamic offset list
    row = header + sizeof(int) * rf->dynamic_cols_count;

    // copy static col
    for (int i = 0; i < rf->static_cols_count; i++) {
        // 获取字段长度
        memcpy(&col_val_len, row, sizeof(int));
        // 获取当前字段在所有字段的序号(按定义时顺序)
        col_num = get_col_num_by_col_name(rf, rf->static_cols_name[i]);
        qr[col_num]->type = rf->cols_fmt[col_num].type;
        qr[col_num]->data = smalloc(col_val_len);
        // 复制字段内容
        memcpy(qr[col_num]->data, row + sizeof(int), col_val_len);
        // next static record
        row += rf->cols_fmt[col_num].len + sizeof(int);
    }
}

void destory_query_result(QueryResultVal* qrv)
{
    free(qrv->data);
}

/**
 *  cvs必须跟列定义时的顺序一致, 不支持null
 */
int insert_row(DB* d, Ast* a)
{
    assert(a->kind == AST_INSERT);

    Page* page;
    int size;
    Table* t;
    Ast *table_name, *row_list, *val_list;

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

        size = cacl_serialized_row_len(t->row_fmt, val_list);
        page = find_free_page(t, size);
        serialize_row(get_page_tail(page), t->row_fmt, val_list);
        page->header.row_count++;
        page->header.tail += size;
        t->row_count++;
        t->free_map[page->header.page_num] -= size;
    }

    return 1;
}

int cmd_compare_eq(Ast* op1, Ast* op2)
{
    return 1;
}

int get_where_expr_res(Table* t, void* row, Ast* where_expr)
{
    assert(where_expr->kind == AST_WHERE_EXP);

    Ast *op1, *op2;
    ExprOp op;

    op1 = where_expr->child[0];
    op2 = where_expr->child[1];
    op = where_expr->attr;

    if (op == E_EQ) {
        return cmd_compare_eq(op1, op2);
    }

    return 0;
}

QueryResult* filter_row(Table* t, Cursor* c, Ast* expect_cols, Ast* where_expr)
{
    QueryResult* qr;
    QueryResultVal* qrv;

    while (!cursor_is_end(c)) {
        if (get_where_expr_res(t, cursor_value(c), where_expr) == 1) {
            qr = smalloc(expect_cols->children * sizeof(QueryResultVal*));
            // 符合条件,返回这条数据
            for (int i = 0; i < expect_cols->children; i++) {
                if (GET_AV_TYPE(expect_cols->child[i]) == AVT_STR) {
                    // 直接返回这个字符串
                    qrv->type = C_CHAR;
                    qrv->data = strdup(GET_AV_STR(expect_cols->child[i]));
                } else if (GET_AV_TYPE(expect_cols->child[i]) == AVT_DOUBLE) {
                    qrv->type = C_INT;
                    qrv->data = smalloc(sizeof(double));
                    copy_ast_val(qrv->data, (AstVal*)(expect_cols->child[i]));
                } else if (GET_AV_TYPE(expect_cols->child[i]) == AVT_INT) {
                    qrv->type = C_INT;
                    qrv->data = smalloc(sizeof(int));
                    copy_ast_val(qrv->data, (AstVal*)(expect_cols->child[i]));
                } else {
                    // TODO: select *
                    qrv = get_col_val(cursor_value(c), t->row_fmt, GET_AV_STR(expect_cols->child[i]));
                }

                qr[i] = qrv;
            }

            // 移动指针
            cursor_next(c);
            return qr;
        } else {
            // 继续下一条
            cursor_next(c);
        }
    }

    return NULL;
}

int check_and_get_select_col_count(Table* t, Ast* expect_cols)
{
    int count = 0;

    for (int i = 0; i < expect_cols->children; i++) {
        if (GET_AV_TYPE(expect_cols->child[i]) == AVT_ID) {
            if (strcmp(GET_AV_STR(expect_cols->child[i]), "*") == 0) {
                count += t->row_fmt->origin_cols_count;
            } else {
                if (get_col_num_by_col_name(t->row_fmt, GET_AV_STR(expect_cols->child[i])) == -1) {
                    printf("col %s not exist\n", GET_AV_STR(expect_cols->child[i]));
                    return -1;
                }
            }
        } else {
            count++;
        }
    }

    return count;
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
    c = cursor_init(t);

    *col_count = check_and_get_select_col_count(t, expect_cols);

    if (*col_count == -1) {
        *row_count = 0;
        return NULL;
    }

    while ((qr = filter_row(t, c, expect_cols, where_top_list->child[0])) != NULL) {
        (*row_count)++;
        qrl = realloc(qrl, (*row_count) * sizeof(QueryResult*));
        qrl[(*row_count) - 1] = qr;
    }

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