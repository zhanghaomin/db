#include "include/table.h"
#include <unistd.h>

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

void* get_page_tail(Page* p)
{
    return (void*)(p->data + p->header.tail);
}