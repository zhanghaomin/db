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
            new_page->header.dir_cnt = 0;
            t->max_page_num = page_num;
        } else { // 旧数据
            read(t->data_fd, new_page, PAGE_SIZE);
        }
    }

    return t->pager->pages[page_num];
}

static int get_page_header_size()
{
    return sizeof(Page);
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

inline int get_page_dir_cnt(Page* p)
{
    return p->header.dir_cnt;
}

inline int get_page_num(Page* p)
{
    return p->header.page_num;
}

static inline int get_sizeof_dir()
{
    return sizeof(int) * 2;
}

inline void set_dir_info(Page* p, int dir_num, int is_delete, int row_offset)
{
    memcpy(p->data + get_sizeof_dir() * (dir_num), &is_delete, sizeof(int));
    memcpy(p->data + get_sizeof_dir() * (dir_num) + sizeof(int), &row_offset, sizeof(int));
}

inline void get_dir_info(Page* p, int dir_num, int* is_delete, int* row_offset)
{
    if (is_delete != NULL) {
        memcpy(is_delete, p->data + get_sizeof_dir() * (dir_num), sizeof(int));
    }
    if (row_offset != NULL) {
        memcpy(row_offset, p->data + get_sizeof_dir() * (dir_num) + sizeof(int), sizeof(int));
    }
}

static inline int get_last_page_dir_num(Page* p)
{
    return get_page_dir_cnt(p) - 1;
}

// | header | page_directory | ... | recs |
// page_directory => | is_delete | offset | ...
Page* reserve_new_row_space(Table* t, int size, int* dir_num)
{
    Page* find_page;
    int offset;

    for (int i = 0; i <= MAX_PAGE_CNT_P_TABLE; i++) {
        // 预留空间给page_directory
        if (size + get_sizeof_dir() <= t->free_map[i]) {
            find_page = get_page(t, i);
            *dir_num = get_page_dir_cnt(find_page);

            if (*dir_num == 0) {
                offset = PAGE_SIZE - get_page_header_size();
            } else {
                get_dir_info(find_page, get_last_page_dir_num(find_page), NULL, &offset);
            }

            offset -= size;
            set_dir_info(find_page, find_page->header.dir_cnt++, 0, offset);
            t->free_map[i] -= (size + get_sizeof_dir());
            return find_page;
        }
    }

    // never happen
    printf("page no space\n");
    exit(-1);
}