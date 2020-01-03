#include "include/table.h"
#include <unistd.h>

static int get_page_header_size();

inline void* row_real_pos(Page* p, int dir_num)
{
    int offset;
    get_dir_info(p, dir_num, NULL, &offset);
    return (void*)(p->data + offset);
}

inline int row_is_delete(Page* p, int dir_num)
{
    int is_delete;
    get_dir_info(p, dir_num, &is_delete, NULL);
    return is_delete;
}

inline void set_row_deleted(Page* p, int dir_num)
{
    int offset;
    get_dir_info(p, dir_num, NULL, &offset);
    set_dir_info(p, dir_num, 1, offset);
}

inline int get_row_len(Page* p, int dir_num)
{
    return ((RowHeader*)row_real_pos(p, dir_num))->row_len;
}

int get_page_cnt(Pager* pr)
{
    return pr->header.page_cnt;
}

static void set_page_cnt(Pager* pr, int page_cnt)
{
    pr->header.page_cnt = page_cnt;
}

Pager* init_pager(int fd)
{
    Pager* pr;
    off_t length;
    pr = scalloc(sizeof(Pager), 1);
    pr->data_fd = fd;

    length = lseek(fd, 0, SEEK_END);

    if (length > PAGE_SIZE) {
        lseek(pr->data_fd, 0, SEEK_SET);
        read(pr->data_fd, &pr->header, sizeof(pr->header));
    }

    return pr;
}

int get_page_free_space(Table* t, Pager* pr, int page_num)
{
    if (page_num < pr->header.page_cnt) {
        return get_page_free_space_stored(t, page_num);
    } else {
        return PAGE_SIZE - get_page_header_size();
    }
}

Page* get_page(Table* t, Pager* pr, int page_num)
{
    Page* new_page;

    if (pr->pages[page_num] == NULL) {
        new_page = scalloc(PAGE_SIZE, 1);
        pr->pages[page_num] = new_page;

        if (page_num >= get_page_cnt(pr)) { // 新空间,不需要读磁盘
            new_page->header.page_num = page_num;
            new_page->header.dir_cnt = 0;
            set_page_cnt(pr, page_num + 1);
            insert_table_free_space(t, page_num, PAGE_SIZE - get_page_header_size());
        } else { // 旧数据
            if (lseek(pr->data_fd, sizeof(pr->header) + PAGE_SIZE * page_num, SEEK_SET) == -1) {
                return NULL;
            }
            read(pr->data_fd, new_page, PAGE_SIZE);
        }
    }

    return pr->pages[page_num];
}

static int get_page_header_size()
{
    return sizeof(Page);
}

int flush_pager_header(Pager* pr)
{
    if (lseek(pr->data_fd, 0, SEEK_SET) == -1) {
        return -1;
    }

    if (write(pr->data_fd, &pr->header, sizeof(pr->header)) == -1) {
        return -1;
    }

    return 0;
}

int flush_page(Pager* pr, int page_num)
{
    if (pr->pages[page_num] == NULL) {
        return 0;
    }

    if (lseek(pr->data_fd, sizeof(pr->header) + PAGE_SIZE * page_num, SEEK_SET) == -1) {
        return -1;
    }

    if (write(pr->data_fd, pr->pages[page_num], PAGE_SIZE) == -1) {
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

inline int get_sizeof_dir()
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

static Page* find_free_space(Table* t, Pager* pr, int size, int* dir_num)
{
    Page* find_page;

    for (int i = 0; i <= MAX_PAGE_CNT_P_TABLE; i++) {
        // 预留空间给page_directory
        if (size + get_sizeof_dir() <= get_page_free_space(t, pr, i)) {
            find_page = get_page(t, pr, i);
            *dir_num = get_page_dir_cnt(find_page);
            return find_page;
        }
    }

    // never happen
    printf("page no space\n");
    exit(-1);
}

int write_row_head(Page* p, void* data, int size)
{
    int offset;
    offset = PAGE_SIZE - get_page_header_size();
    offset -= size;
    memcpy(row_real_pos(p, 0), data, size);
    set_dir_info(p, p->header.dir_cnt++, 0, offset);
    return 1;
}

int write_row_to_page(Table* t, Pager* pr, void* data, int size)
{
    int dir_num, offset;
    Page* p;

    p = find_free_space(t, pr, size, &dir_num);

    if (dir_num == 0) {
        offset = PAGE_SIZE - get_page_header_size();
    } else {
        get_dir_info(p, get_last_page_dir_num(p), NULL, &offset);
    }

    offset -= size;
    memcpy(row_real_pos(p, dir_num), data, size);
    set_dir_info(p, p->header.dir_cnt++, 0, offset);
    incr_table_free_space(t, p->header.page_num, -(size + get_sizeof_dir()));
    return 1;
}

int replace_row(Table* t, Page* p, int dir_num, void* data, int len)
{
    int old_size, free_space, move_start_offset, move_end_offset, move_size, origin_offset, origin_is_delete, res;
    void *move_src, *move_dest, *origin_store;

    old_size = get_row_len(p, dir_num);
    free_space = get_page_free_space(t, t->pager, get_page_num(p));

    if (free_space < len - old_size) { // 当前页空间不足
        // 删除原有元素, 并新增
        set_row_deleted(p, dir_num);
        return write_row_to_page(t, t->pager, data, len);
    }

    origin_store = smalloc(len);
    memcpy(origin_store, row_real_pos(p, dir_num), len);

    // 移动
    if (len != old_size) {
        // 需要移动之后插入的元素
        move_start_offset = get_row_offset(p, get_last_page_dir_num(p));
        move_end_offset = get_row_offset(p, dir_num);
        move_size = move_end_offset - move_start_offset;
        move_src = (void*)p->data + move_start_offset;
        move_dest = move_src - (len - old_size);
        memmove(move_dest, (void*)p->data + move_start_offset, move_size);

        // 从当前dir开始, 每个减size - old_size
        for (int i = dir_num; i < get_page_dir_cnt(p); i++) {
            get_dir_info(p, i, &origin_is_delete, &origin_offset);
            origin_offset -= (len - old_size);
            set_dir_info(p, i, origin_is_delete, origin_offset);
        }

        incr_table_free_space(t, p->header.page_num, -(len - old_size));
    }

    memcpy(row_real_pos(p, dir_num), data, len);
    res = memcmp(origin_store, data, len);
    free(origin_store);
    return res;
}