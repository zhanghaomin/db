#include "table.h"
#include "util.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// TODO
table* table_init(char* name)
{
    table* t;
    t = scalloc(sizeof(table), 1);
    t->data_fd = -1; // TODO
    t->row_fmt = NULL;
    t->max_page_num = 0;
    t->name = strdup(name);
    t->row_count = 0;
    return t;
}

void* cursor_value(cursor* c)
{
    return (void*)(c->page->data + c->offset);
}

int cursor_is_end(cursor* c)
{
    return c->row_num == c->table->row_count;
}

static int get_row_len(void* row_start)
{
    row_header rh;
    memcpy(&rh, row_start, sizeof(rh));
    return rh.row_len;
}

page* get_page(pager* p, int page_num)
{
    page* new_page;

    if (p->pages[page_num] == NULL) {
        new_page = smalloc(PAGE_SIZE);
        new_page->header.page_num = page_num;
        new_page->header.row_count = 0;
        new_page->header.tail = 0;
        p->pages[page_num] = new_page;

        if (page_num > p->table->max_page_num) {
            p->table->max_page_num = page_num;
        }
    }

    return p->pages[page_num];
}

int flush_page(pager* p, int page_num)
{
    if (p->pages[page_num] == NULL) {
        return 0;
    }

    if (lseek(p->table->data_fd, PAGE_SIZE * page_num, SEEK_SET) == -1) {
        return -1;
    }

    if (write(p->table->data_fd, p->pages[page_num], PAGE_SIZE) == -1) {
        return -1;
    }

    return 0;
}

static int cursor_reach_page_end(cursor* c)
{
    return c->page_row_num == c->page->header.row_count;
}

void cursor_next(cursor* c)
{
    if (cursor_is_end(c)) {
        return;
    }

    if (cursor_reach_page_end(c)) {
        c->page = get_page(c->pager, c->page->header.page_num + 1);
        c->offset = 0;
        c->page_row_num = 0;
    } else {
        c->offset += get_row_len(cursor_value(c));
        c->page_row_num++;
    }

    c->row_num++;
    return;
}

void cursor_rewind(cursor* c)
{
    c->page = get_page(c->pager, 0);
    c->offset = 0;
    c->page_row_num = 0;
    c->row_num = 0;
}

void println_table_col_name(table* t)
{
    for (int i = 0; i < t->row_fmt->origin_cols_count; i++) {
        printf("%s\t", t->row_fmt->origin_cols_name[i]);
    }

    printf("\n");
}

void println_row(col_value** cvs, int cvs_len)
{
    for (int i = 0; i < cvs_len; i++) {
        if (cvs[i]->type == VARCHAR || cvs[i]->type == CHAR) {
            printf("%s\t", cvs[i]->data);
        } else if (cvs[i]->type == INT) {
            printf("%d\t", *(int*)(cvs[i]->data));
        } else if (cvs[i]->type == DOUBLE) {
            printf("%f\t", *(double*)(cvs[i]->data));
        }
    }

    printf("\n");
}

void traverse_table(cursor* c)
{
    col_value** cvs;
    int cvs_len;

    cursor_rewind(c);
    println_table_col_name(c->table);
    cvs_len = c->table->row_fmt->origin_cols_count;
    cvs = smalloc(cvs_len * sizeof(col_value*));

    for (int i = 0; i < cvs_len; i++) {
        cvs[i] = smalloc(sizeof(col_value));
    }

    while (!cursor_is_end(c)) {

        unserialize_row(cursor_value(c), c->table->row_fmt, cvs);
        println_row(cvs, cvs_len);

        for (int i = 0; i < cvs_len; i++) {
            destory_col_val(cvs[i]);
        }

        cursor_next(c);
    }

    for (int i = 0; i < cvs_len; i++) {
        free(cvs[i]);
    }
}

void* find_free_space(table* t, pager* p, int size)
{
    page* find_page;
    int i = 0;

    for (; i <= t->max_page_num; i++) {
        if (size <= t->free_map[i]) {
            find_page = get_page(p, i);
            return (void*)(find_page->data + find_page->header.tail);
        }
    }

    // never happen
    exit(-1);
}

col_fmt* get_col_fmt_by_col_name(row_fmt* rf, char* col_name)
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

int get_static_col_num_by_col_name(row_fmt* rf, char* col_name)
{
    for (int i = 0; i < rf->static_cols_count; i++) {
        if (strcmp(rf->static_cols_name[i], col_name) == 0) {
            return i;
        }
    }

    return -1;
}

int get_dynamic_col_num_by_col_name(row_fmt* rf, char* col_name)
{
    for (int i = 0; i < rf->dynamic_cols_count; i++) {
        if (strcmp(rf->dynamic_cols_name[i], col_name) == 0) {
            return i;
        }
    }

    return -1;
}

int get_col_num_by_col_name(row_fmt* rf, char* col_name)
{
    for (int i = 0; i < rf->origin_cols_count; i++) {
        if (strcmp(rf->origin_cols_name[i], col_name) == 0) {
            return i;
        }
    }

    return -1;
}

int cacl_serialized_row_len(row_fmt* rf, col_value** cvs, int col_count)
{
    int result = 0;
    col_fmt* cf;

    result += sizeof(int);

    for (int i = 0; i < col_count; i++) {
        cf = &rf->cols_fmt[i];

        if (cf == NULL) {
            return -1;
        }

        if (cf->is_dynamic) { // 动态属性需要额外空间储存位置
            result += sizeof(int);
        }

        result += sizeof(int);
        result += cvs[i]->len;
    }

    return result;
}

/*
    || row_header || 1st dynamic offset || 2nd dynamic offset || ... || 1st reclen | 1st static rec || 2nd reclen | 2nd static rec || ... || 1st dynamic reclen | 1st dynamic rec || 2nd dynamic reclen | 2nd dynamic rec || ...
                            |______________________________________________________________________________________________________________|
                                                  |________________________________________________________________________________________________________________________________|
     偏移量都是相对header的
*/
int serialize_row(void* store, row_fmt* rf, col_value** cvs, int col_count)
{
    col_fmt* cf;
    row_header rh;
    rh.row_len = 0;
    void *dynamic_offset_store, *data_start;
    int offset = 0;

    data_start = store + sizeof(int);
    dynamic_offset_store = data_start;
    store += sizeof(int) + sizeof(int) * rf->dynamic_cols_count;

    // 写入静态字段
    for (int i = 0; i < col_count; i++) {
        if (!cf->is_dynamic) {
            cf = &rf->cols_fmt[i];

            if (cf == NULL) {
                return -1;
            }

            memcpy(store, &cvs[i]->len, sizeof(int));
            store += sizeof(int);
            memcpy(store, cvs[i]->data, cvs[i]->len);
            store += cvs[i]->len;
            rh.row_len += sizeof(int);
            rh.row_len += cvs[i]->len;
        }
    }

    // 写入动态字段, 修改偏移量
    for (int i = 0; i < col_count; i++) {
        if (cf->is_dynamic) { // 动态属性需要额外空间储存位置
            cf = &rf->cols_fmt[i];

            if (cf == NULL) {
                return -1;
            }

            offset = store - data_start;
            memcpy(dynamic_offset_store, &offset, sizeof(int));
            dynamic_offset_store += sizeof(int);
            memcpy(store, &cvs[i]->len, sizeof(int));
            store += sizeof(int);
            memcpy(store, cvs[i]->data, cvs[i]->len);
            store += cvs[i]->len;
            rh.row_len += 2 * sizeof(int);
            rh.row_len += cvs[i]->len;
        }
    }

    // 写入header
    memcpy(data_start - sizeof(row_header), &rh, sizeof(row_header));
    return 0;
}

int get_col_val(void* row, row_fmt* rf, char* col_name, col_value* cv)
{
    int col_num, dynamic_col_num, static_col_num;
    void* col;
    col_fmt cf;
    col_num = get_col_num_by_col_name(rf, col_name);

    if (col_num == -1) {
        return -1;
    }

    cf = rf->cols_fmt[col_num];
    cv->type = cf.type;

    if (cf.is_dynamic) {
        dynamic_col_num = get_dynamic_col_num_by_col_name(rf, col_name);
        col = row + sizeof(int) * dynamic_col_num;
    } else {
        static_col_num = get_static_col_num_by_col_name(rf, col_name);
        col = row + sizeof(int) * rf->dynamic_cols_count + static_col_num * sizeof(int);
    }

    memcpy(&cv->len, col, sizeof(int));
    cv->data = smalloc(cv->len);
    memcpy(&cv->data, col + sizeof(int), cv->len);
    return 0;
}

/*
 * 返回反序列化好的字段数组, 顺序和定义时的一致
*/
void unserialize_row(void* row, row_fmt* rf, col_value** cvs)
{
    void* header;
    int offset, col_val_len, col_num;

    // skip header
    row += sizeof(row_header);
    header = row;

    // copy dynamic col
    for (int i = 0; i < rf->dynamic_cols_count; i++) {
        // 获取动态字段偏移量
        memcpy(&offset, row, sizeof(int));
        // 获取字段长度
        memcpy(&col_val_len, header + offset, sizeof(int));
        // 获取当前字段在所有字段的序号(按定义时顺序)
        col_num = get_col_num_by_col_name(rf, rf->dynamic_cols_name[i]);
        cvs[col_num]->type = rf->cols_fmt[col_num].type;
        cvs[col_num]->len = col_val_len;
        cvs[col_num]->data = smalloc(col_val_len);
        // 复制字段内容
        memcpy(cvs[col_num]->data, header + offset + sizeof(int), col_val_len);
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
        col_num = get_col_num_by_col_name(rf, rf->dynamic_cols_name[i]);
        cvs[col_num]->type = rf->cols_fmt[col_num].type;
        cvs[col_num]->len = col_val_len;
        cvs[col_num]->data = smalloc(col_val_len);
        // 复制字段内容
        memcpy(cvs[col_num]->data, row + sizeof(int), col_val_len);
        // next static record
        row += rf->cols_fmt[col_num].len + sizeof(int);
    }
}

void destory_col_val(col_value* cv)
{
    free(cv->data);
}

/**
 *  cvs必须跟列定义时的顺序一致, 不支持null
 */
void insert_row(table* t, pager* p, col_value** cvs, int col_count)
{
    void* store;
    int size;

    if (col_count != t->row_fmt->origin_cols_count) {
        sys_err("col count conflict");
    }

    size = cacl_serialized_row_len(t->row_fmt, cvs, col_count);

    if (size == -1) {
        sys_err("conflict col");
    }

    store = find_free_space(t, p, size);

    if (serialize_row(store, t->row_fmt, cvs, col_count) == -1) {
        sys_err("conflict col");
    }

    return;
}

int do_cmp_op(col_value* op1, input_literal* op2, cmp_op op)
{
    // maybe we need tranfer type
}

int where_condition_pass(void* row, row_fmt* rf, where_stmt* ws)
{
    col_value cv;
    int res;

    if (ws->is_leaf) {
        get_col_val(row, rf, ws->children[0].val->data, &cv);
        res = do_cmp_op(&cv, ws->children[1].val, ws->op.compare);
        destory_col_val(&cv);
        return res;
    }

    if (ws->op.logic == W_AND) {
        return where_condition_pass(row, rf, ws->children[0].stmt) && where_condition_pass(row, rf, ws->children[1].stmt);
    } else if (ws->op.logic == W_OR) {
        return where_condition_pass(row, rf, ws->children[0].stmt) || where_condition_pass(row, rf, ws->children[1].stmt);
    }

    // never happen
    exit(-1);
}

result_rows select_row(cursor* c, char** expect_cols, int expect_cols_count, where_stmt* ws, int* row_count)
{
    int rc = 0;
    result_rows rows;

    rows = smalloc(sizeof(result_row));
    cursor_rewind(c);

    while (!cursor_is_end(c)) {
        if (where_condition_pass(cursor_value(c), c->table->row_fmt, ws)) {
            rows = realloc(rows, rc * sizeof(result_row));
            rows[rc] = smalloc(sizeof(col_value) * expect_cols_count);

            for (int i = 0; i < expect_cols_count; i++) {
                get_col_val(cursor_value(c), c->table->row_fmt, expect_cols[i], &rows[rc][i]);
            }

            rc++;
        }
    }

    return rows;
}

// void update_row