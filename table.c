#include "table.h"
#include "util.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void traverse_table(cursor* c)
{
    cursor_rewind(c);

    while (!cursor_is_end(c)) {
        printf("%p\n", cursor_value(c));
        cursor_next(c);
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

int cacl_serialized_row_len(row_fmt* rf, col_value* cvs, int col_count)
{
    int result = 0;
    col_fmt* cf;

    result += sizeof(int);

    for (int i = 0; i < col_count; i++) {
        cf = get_col_fmt_by_col_name(rf, cvs[i].name);

        if (cf == NULL) {
            return -1;
        }

        if (cf->is_dynamic) { // 动态属性需要额外空间储存位置
            result += sizeof(int);
        }

        result += sizeof(int);
        result += cvs[i].len;
    }

    return result;
}

/*
    || row_header || 1st dynamic offset || 2nd dynamic offset || ... || 1st reclen | 1st static rec || 2nd reclen | 2nd static rec || ... || 1st dynamic reclen | 1st dynamic rec || 2nd dynamic reclen | 2nd dynamic rec || ...
                            |______________________________________________________________________________________________________________|
                                                  |________________________________________________________________________________________________________________________________|
     偏移量都是相对header的
*/
int serialize_row(void* store, row_fmt* rf, col_value* cvs, int col_count)
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
        cf = get_col_fmt_by_col_name(rf, cvs[i].name);

        if (cf == NULL) {
            return -1;
        }

        if (!cf->is_dynamic) {
            memcpy(store, &cvs[i].len, sizeof(int));
            store += sizeof(int);
            memcpy(store, cvs[i].data, cvs[i].len);
            store += cvs[i].len;
            rh.row_len += sizeof(int);
            rh.row_len += cvs[i].len;
        }
    }

    // 写入动态字段, 修改偏移量
    for (int i = 0; i < col_count; i++) {
        cf = get_col_fmt_by_col_name(rf, cvs[i].name);

        if (cf == NULL) {
            return -1;
        }

        if (cf->is_dynamic) { // 动态属性需要额外空间储存位置
            offset = store - data_start;
            memcpy(dynamic_offset_store, &offset, sizeof(int));
            dynamic_offset_store += sizeof(int);
            memcpy(store, &cvs[i].len, sizeof(int));
            store += sizeof(int);
            memcpy(store, cvs[i].data, cvs[i].len);
            store += cvs[i].len;
            rh.row_len += 2 * sizeof(int);
            rh.row_len += cvs[i].len;
        }
    }

    // 写入header
    memcpy(data_start - sizeof(row_header), &rh, sizeof(row_header));
    return 0;
}

void insert_row(table* t, pager* p, col_value* cvs, int col_count)
{
    void* store;
    int size;
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

// void update_row