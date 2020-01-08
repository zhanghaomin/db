#include "include/lru_list.h"
#include "include/table.h"
#include <unistd.h>

static int get_page_header_size();

// | header | page_directory | ... | recs |
// page_directory => | is_delete | offset | ...
// 不用page_directory时，一个记录的增大会导致所有记录后移，直接依赖他们偏移量的地方都需要更改（更新索引会很麻烦）
// 使用page_directory在更新时，如果空间足够，那么移动他后面的元素，外面的引用不需要修改，如果空间不够，删除原来的记录，寻找新的空间插入，外面的引用只需要更改当前记录
// 删除只标记，不删除，等剩余空间太大时再删除
typedef struct {
    struct {
        int page_num; // 页号
        int dir_cnt; // 页目录项数
        int fd;
    } header;
    char data[];
} Page;

typedef struct {
    LruList* pages; // 表里的page集合
} GlobalPager;

GlobalPager* GP;

static inline void get_dir_info(Page* p, int dir_num, int* is_delete, int* row_offset)
{
    if (is_delete != NULL) {
        memcpy(is_delete, p->data + get_sizeof_dir() * (dir_num), sizeof(int));
    }
    if (row_offset != NULL) {
        memcpy(row_offset, p->data + get_sizeof_dir() * (dir_num) + sizeof(int), sizeof(int));
    }
}

static inline void set_dir_info(Page* p, int dir_num, int is_delete, int row_offset)
{
    memcpy(p->data + get_sizeof_dir() * (dir_num), &is_delete, sizeof(int));
    memcpy(p->data + get_sizeof_dir() * (dir_num) + sizeof(int), &row_offset, sizeof(int));
}

static inline void* row_real_pos(Page* p, int dir_num)
{
    int offset;
    get_dir_info(p, dir_num, NULL, &offset);
    return (void*)(p->data + offset);
}

static inline int get_row_len(Page* p, int dir_num)
{
    return ((RowHeader*)row_real_pos(p, dir_num))->row_len;
}

static void set_page_cnt(Pager* pr, int page_cnt)
{
    pr->header.page_cnt = page_cnt;
}

static Page* get_page(Table* t, int page_num)
{
    Page* new_page;
    char key[1024];
    Pager* pr;

    pr = t->pager;
    sprintf(key, "%s#~%d", t->name, page_num);

    if ((new_page = lru_list_get(GP->pages, key)) == NULL) {
        new_page = scalloc(PAGE_SIZE, 1);
        lru_list_add(GP->pages, key, new_page);

        if (page_num >= get_page_cnt(pr)) {
            new_page->header.page_num = page_num;
            new_page->header.dir_cnt = 0;
            new_page->header.fd = pr->data_fd;
            set_page_cnt(pr, page_num + 1);
            insert_table_free_space(t, page_num, PAGE_SIZE - get_page_header_size());
            // TODO:这里可能要再读取一次
            return get_page(t, page_num);
        } else { // 旧数据
            if (lseek(pr->data_fd, sizeof(pr->header) + PAGE_SIZE * page_num, SEEK_SET) == -1) {
                return NULL;
            }
            read(pr->data_fd, new_page, PAGE_SIZE);
            new_page->header.fd = pr->data_fd;
        }
    }

    return new_page;
}

static int get_page_free_space(Table* t, int page_num)
{
    if (page_num < t->pager->header.page_cnt) {
        return get_page_free_space_stored(t, page_num);
    } else {
        return PAGE_SIZE - get_page_header_size();
    }
}

static int get_page_header_size()
{
    return sizeof(Page);
}

static inline int get_last_page_dir_num(Page* p)
{
    return p->header.dir_cnt - 1;
}

static inline int get_row_offset(Page* p, int dir_num)
{
    int offset;
    get_dir_info(p, dir_num, NULL, &offset);
    return offset;
}

static Page* find_free_space(Table* t, int size, int* dir_num)
{
    Page* find_page;

    for (int i = 0;; i++) {
        // 预留空间给page_directory
        if (size + get_sizeof_dir() <= get_page_free_space(t, i)) {
            find_page = get_page(t, i);
            *dir_num = find_page->header.dir_cnt;
            return find_page;
        }
    }

    return NULL;
}

inline int row_is_delete(Table* t, int page_num, int dir_num)
{
    int is_delete;
    Page* p = get_page(t, page_num);
    get_dir_info(p, dir_num, &is_delete, NULL);
    return is_delete;
}

inline void set_row_deleted(Table* t, int page_num, int dir_num)
{
    int offset;
    Page* p = get_page(t, page_num);
    get_dir_info(p, dir_num, NULL, &offset);
    set_dir_info(p, dir_num, 1, offset);
}

inline int get_page_cnt(Pager* pr)
{
    return pr->header.page_cnt;
}

void destory_GP()
{
    lru_list_destory(GP->pages);
    free(GP);
}

void clean_page(void* data)
{
    Page* p;
    Pager pr;
    p = (Page*)data;

    if (lseek(p->header.fd, sizeof(pr.header) + PAGE_SIZE * p->header.page_num, SEEK_SET) == -1) {
        goto free;
        return;
    }

    if (write(p->header.fd, p, PAGE_SIZE) == -1) {
        goto free;
        return;
    }

free:
    free(data);
}

void init_GP(int size)
{
    GP = smalloc(sizeof(GlobalPager));
    GP->pages = lru_list_init(size, NULL, clean_page);
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

inline int get_page_dir_cnt(Table* t, int page_num)
{
    Page* p = get_page(t, page_num);
    return p->header.dir_cnt;
}

inline int get_sizeof_dir()
{
    return sizeof(int) * 2;
}

int write_row_head(Table* t, int page_num, void* data, int size)
{
    int offset;
    Page* p = get_page(t, page_num);
    offset = PAGE_SIZE - get_page_header_size();
    offset -= size;
    memcpy(p->data + offset, data, size);
    set_dir_info(p, p->header.dir_cnt++, 0, offset);
    return 1;
}

int write_row_to_page(Table* t, void* data, int size)
{
    int dir_num, offset, page_num;
    Page* p;

    p = find_free_space(t, size, &dir_num);

    if (dir_num == 0) {
        offset = PAGE_SIZE - get_page_header_size();
    } else {
        get_dir_info(p, get_last_page_dir_num(p), NULL, &offset);
    }

    offset -= size;
    memcpy(p->data + offset, data, size);
    set_dir_info(p, p->header.dir_cnt, 0, offset);
    page_num = p->header.page_num;
    incr_table_free_space(t, page_num, -(size + get_sizeof_dir()));
    // TODO: 可能需要再读
    p = get_page(t, page_num);
    p->header.dir_cnt++;
    return 1;
}

void* copy_row_raw_data(Table* t, int page_num, int dir_num)
{
    Page* p;
    void* data;
    int len;
    p = get_page(t, page_num);
    len = get_row_len(p, dir_num);
    data = smalloc(len);
    memcpy(data, row_real_pos(p, dir_num), len);
    return data;
}

int replace_row(Table* t, int page_num, int dir_num, void* data, int len)
{
    int old_size, free_space, move_start_offset, move_end_offset, move_size, origin_offset, origin_is_delete, res, has_move;
    void *move_src, *move_dest, *origin_store;
    Page* p;

    free_space = get_page_free_space(t, page_num);
    p = get_page(t, page_num);
    old_size = get_row_len(p, dir_num);

    if (free_space < len - old_size) { // 当前页空间不足
        // 删除原有元素, 并新增
        set_row_deleted(t, page_num, dir_num);
        return write_row_to_page(t, data, len);
    }

    origin_store = smalloc(len);
    memcpy(origin_store, row_real_pos(p, dir_num), len);

    has_move = 0;
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
        for (int i = dir_num; i < t->pager->header.page_cnt; i++) {
            get_dir_info(p, i, &origin_is_delete, &origin_offset);
            origin_offset -= (len - old_size);
            set_dir_info(p, i, origin_is_delete, origin_offset);
        }

        has_move = 1;
    }

    memcpy(row_real_pos(p, dir_num), data, len);
    res = memcmp(origin_store, data, len);
    free(origin_store);

    if (has_move) {
        incr_table_free_space(t, p->header.page_num, -(len - old_size));
    }

    return res != 0;
}