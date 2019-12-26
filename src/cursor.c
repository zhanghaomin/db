#include "include/table.h"

static int get_row_len(void* row_start)
{
    RowHeader rh;
    memcpy(&rh, row_start, sizeof(rh));
    return rh.row_len;
}

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