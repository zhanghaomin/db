#include "pager.h"
#include "util.h"
#include <fcntl.h>
#include <stdlib.h>

void* get_page(table_pager* p, int page_no)
{
    void* data;

    if ((data = p->pages[page_no]) == NULL) {
        data = malloc(PAGE_SIZE);

        if (data == NULL) {
            sys_err("malloc");
        }

        if (lseek(p->fd, page_no * PAGE_SIZE, SEEK_SET) == -1) {
            sys_err("lseek");
        }

        if (read(p->fd, data, PAGE_SIZE) == -1) {
            sys_err("read");
        }

        p->pages[page_no] = data;
    }

    return data;
}

int flush_page(table_pager* p, int page_no)
{
}