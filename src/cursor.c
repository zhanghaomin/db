#include "include/table.h"

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
    return row_real_pos(c->page, c->page_dir_num);
}

int cursor_is_end(Cursor* c)
{
    return c->row_num == get_table_row_cnt(c->table);
}

static int cursor_reach_page_end(Cursor* c)
{
    return c->page_dir_num == get_page_dir_cnt(c->page);
}

void cursor_next(Cursor* c, int skip_delete)
{
    if (cursor_is_end(c)) {
        return;
    }

retry:
    c->page_dir_num++;

    if (cursor_reach_page_end(c)) {
        c->page = get_page(c->table, get_page_num(c->page) + 1);
        c->page_dir_num = 0;
    } else if (skip_delete && row_is_delete(c->page, c->page_dir_num)) {
        goto retry;
    }

    c->row_num++;
    return;
}

void cursor_rewind(Cursor* c)
{
    c->page = get_page(c->table, 0);
    c->page_dir_num = 0;
    c->row_num = 0;
}