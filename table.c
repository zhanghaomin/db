#include "table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO
table* table_init(char* name)
{
    table* t;
    t = calloc(sizeof(table), 1);
    t->data_fd = -1; // TODO
    t->format = NULL;
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

static int row_len(void* row_start)
{
    int len;
    memcpy(&len, row_start, sizeof(int));
    return len;
}

page* get_page(pager* p, int page_num)
{
    page* new_page;

    if (p->pages[page_num] == NULL) {
        new_page = malloc(PAGE_SIZE);
        new_page->page_num = page_num;
        new_page->row_count = 0;
        new_page->tail = 0;
        p->pages[page_num] = new_page;

        if (page_num > p->table->max_page_num) {
            p->table->max_page_num = page_num;
        }
    }

    return p->pages[page_num];
}

static int cursor_reach_page_end(cursor* c)
{
    return c->page_row_num == c->page->row_count;
}

void cursor_next(cursor* c)
{
    if (cursor_is_end(c)) {
        return;
    }

    if (cursor_reach_page_end(c)) {
        c->page = get_page(c->pager, c->page->page_num + 1);
        c->offset = 0;
        c->page_row_num = 0;
    } else {
        c->offset += row_len(cursor_value(c));
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
            return (void*)(find_page->data + find_page->tail);
        }
    }

    // never happen
    exit(-1);
}

void insert_row(table* t, pager* p, void* row, int size)
{
    void* pos;
    pos = find_free_space(t, p, size);
    memcpy(pos, row, size);
    return;
}

// void update_row