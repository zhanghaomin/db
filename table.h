#ifndef _TABLE_H
#define _TABLE_H

#define PAGE_SIZE 4096
#define MAX_PAGE_CNT_P_TABLE 4096

typedef enum {
    INT,
    DOUBLE,
    CHAR,
    VARCHAR
} col_type;

typedef struct {
    char* name;
    void* data;
    int len;
} col_value;

typedef struct {
    col_type type; // 字段类型
    int is_dynamic; // 是否变长
    int len; // 字段长度
} col_fmt;

typedef struct {
    int row_len;
} row_header;

typedef struct {
    char** static_cols_name; // 定长字段数组
    char** dynamic_cols_name; // 不定长字段数组
    int static_cols_count; // 定长字段数量
    int dynamic_cols_count; // 不定长字段数量
    col_fmt* cols_fmt; // 字段格式数组, 与上面下标对应, 先放静态, 再放动态
} row_fmt;

typedef struct {
    char* name;
    int data_fd;
    row_fmt* row_fmt;
    int max_page_num; // 最大叶号
    int row_count; // 表里数据总行数
    int free_map[MAX_PAGE_CNT_P_TABLE]; // 每页的空闲空间
} table;

typedef struct {
    struct {
        int page_num; // 页号
        int row_count; // 页里有多少行
        int tail; // 相对于page.data
    } header;
    char data[];
} page;

typedef struct {
    page* pages[MAX_PAGE_CNT_P_TABLE]; // 表里的page集合
    table* table;
} pager;

typedef struct {
    pager* pager;
    table* table;
    // row_fmt* row_fmt;
    page* page;
    int offset; // 相对于page.data
    int row_num; // 当前指向第几行
    int page_row_num; // 当前指向page中第几行
} cursor;

#endif