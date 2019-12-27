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

inline Page* cursor_page(Cursor* c)
{
    return c->page;
}

inline int cursor_dir_num(Cursor* c)
{
    return c->page_dir_num;
}

void* cursor_value(Cursor* c)
{
    return row_real_pos(cursor_page(c), cursor_dir_num(c));
}

static inline int cursor_reach_page_end(Cursor* c)
{
    return cursor_dir_num(c) == get_page_dir_cnt(c->page);
}

inline int cursor_is_end(Cursor* c)
{
    return cursor_reach_page_end(c) && get_page_num(cursor_page(c)) == get_table_max_page_num(c->table);
}

inline int cursor_value_is_deleted(Cursor* c)
{
    return row_is_delete(cursor_page(c), cursor_dir_num(c));
}

void cursor_next(Cursor* c)
{
    if (cursor_is_end(c)) {
        return;
    }

    c->page_dir_num++;

    if (cursor_reach_page_end(c)) {
        c->page = get_page(c->table, get_page_num(cursor_page(c)) + 1);
        c->page_dir_num = 0;
    }

    return;
}

void cursor_rewind(Cursor* c)
{
    c->page = get_page(c->table, 0);
    c->page_dir_num = 0;
}