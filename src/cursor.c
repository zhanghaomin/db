#include "include/table.h"

Cursor* cursor_init(Table* t)
{
    Cursor* c;
    c = scalloc(sizeof(Cursor), 1);
    c->table = t;
    c->pager = t->pager;
    c->page_dir_num = 0;
    c->page_num = 0;
    return c;
}

void cursor_destory(Cursor* c)
{
    free(c);
}

inline int cursor_page_num(Cursor* c)
{
    return c->page_num;
}

inline int cursor_dir_num(Cursor* c)
{
    return c->page_dir_num;
}

static inline int cursor_reach_page_end(Cursor* c)
{
    return cursor_dir_num(c) == get_page_dir_cnt(c->table, c->page_num);
}

inline int cursor_is_end(Cursor* c)
{
    return cursor_reach_page_end(c) && c->page_num == get_page_cnt(c->pager) - 1;
}

inline int cursor_value_is_deleted(Cursor* c)
{
    return row_is_delete(c->table, c->page_num, cursor_dir_num(c));
}

void cursor_next(Cursor* c)
{
    c->page_dir_num++;

    if (cursor_reach_page_end(c) && !cursor_is_end(c)) {
        c->page_num++;
        c->page_dir_num = 0;
    }

    return;
}