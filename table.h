#ifndef _TABLE_H
#define _TABLE_H

#define PAGE_SIZE 4096
#define MAX_PAGE_CNT_P_TABLE 4096

typedef struct {
    int type;
    int len;
} column_format;

typedef struct {
    char** origin_columns;
    char** static_columns;
    char** dynamic_columns;
    int static_columns_count;
    int dynamic_columns_count;
    column_format* columns_format;
} row_format;

typedef struct {
    char* name;
    int data_fd;
    row_format* format;
    int max_page_num;
    int row_count;
    int free_map[MAX_PAGE_CNT_P_TABLE];
} table;

typedef struct {
    int page_num;
    int row_count;
    int tail; // 相对于page.data
    char data[];
} page;

typedef struct {
    page* pages[MAX_PAGE_CNT_P_TABLE];
    table* table;
} pager;

typedef struct {
    pager* pager;
    table* table;
    // row_format* row_format;
    page* page;
    int offset; // 相对于page.data
    int row_num;
    int page_row_num;
} cursor;

#endif