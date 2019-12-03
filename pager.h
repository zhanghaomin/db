#ifndef _PAGER_H
#define _PAGER_H

#define MAX_CACHE_SIZE 1 << 20
#define PAGE_SIZE 4096

typedef struct {
    int fd;
    // TODO:lru
    void* pages[MAX_CACHE_SIZE / PAGE_SIZE];
} table_pager;

void* get_page(table_pager* p, int page_no);

#endif
