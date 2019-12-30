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
            if (lseek(t->data_fd, PAGE_SIZE * page_num, SEEK_SET) == -1) {
                return NULL;
            }
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

static inline int get_row_offset(Page* p, int dir_num)
{
    int offset;
    get_dir_info(p, dir_num, NULL, &offset);
    return offset;
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

// 计算剩余空间是否足够, 足够则移动后续元素, 不够则删除原有元素, 并新增
Page* resize_row_space(Table* t, Page* old_page, int* dir_num, int size)
{
    int old_size, free_space, move_start_offset, move_end_offset, move_size, origin_offset, origin_is_delete;
    void *move_src, *move_dest;

    old_size = get_row_len(old_page, *dir_num);
    free_space = t->free_map[get_page_num(old_page)];

    if (free_space < size - old_size) { // 当前页空间不足
        // 删除原有元素, 并新增
        set_row_deleted(old_page, *dir_num);
        return reserve_new_row_space(t, size, dir_num);
    }

    // 移动
    if (size != old_size) {
        // 需要移动之后插入的元素
        move_start_offset = get_row_offset(old_page, get_last_page_dir_num(old_page));
        move_end_offset = get_row_offset(old_page, *dir_num);
        move_size = move_end_offset - move_start_offset;
        move_src = (void*)old_page->data + move_start_offset;
        move_dest = move_src - (size - old_size);
        memmove(move_dest, (void*)old_page->data + move_start_offset, move_size);

        // 从当前dir开始, 每个减size - old_size
        for (int i = *dir_num; i < get_page_dir_cnt(old_page); i++) {
            get_dir_info(old_page, i, &origin_is_delete, &origin_offset);
            origin_offset -= (size - old_size);
            set_dir_info(old_page, i, origin_is_delete, origin_offset);
        }
    }

    return old_page;
}