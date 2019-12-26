#ifndef _TABLE_H
#define _TABLE_H

#define PAGE_SIZE 4096
#define MAX_PAGE_CNT_P_TABLE 4096

#include "ast.h"
#include "ht.h"

typedef struct {
    ColType type;
    void* data;
} QueryResultVal;

typedef QueryResultVal* QueryResult;
typedef QueryResult* QueryResultList;

typedef struct {
    ColType type; // 字段类型
    int is_dynamic; // 是否变长
    int len; // 字段长度
} ColFmt;

typedef struct {
    int row_len;
} RowHeader;

typedef struct {
    char** origin_cols_name; // 原始字段数组, 定义时顺序
    int origin_cols_count; // 原始字段数量
    ColFmt* cols_fmt; // 字段格式数组, 与上面下标对应
    char** static_cols_name; // 定长字段数组
    char** dynamic_cols_name; // 不定长字段数组
    int static_cols_count; // 定长字段数量
    int dynamic_cols_count; // 不定长字段数量
} RowFmt;

// | header | page_directory | ... | recs |
// 不用page_directory时，一个记录的增大会导致所有记录后移，直接依赖他们偏移量的地方都需要更改（更新索引会很麻烦）
// 使用page_directory在更新时，如果空间足够，那么移动他后面的元素，外面的引用不需要修改，如果空间不够，使用溢出块，外面的引用也不需要更改
// 删除只标记，不删除，等剩余空间太大时再删除
typedef struct {
    struct {
        int page_num; // 页号
        int row_count; // 页里有多少行
        int tail; // 相对于page.data
    } header;
    char data[];
} Page;

typedef struct {
    Page* pages[MAX_PAGE_CNT_P_TABLE]; // 表里的page集合
} Pager;

typedef struct {
    HashTable* tables;
} DB;

//
typedef struct {
    char* name;
    int data_fd;
    RowFmt* row_fmt;
    Pager* pager;
    int max_page_num; // 最大叶号
    int row_count; // 表里数据总行数
    int free_map[MAX_PAGE_CNT_P_TABLE]; // 每页的空闲空间
} Table;

typedef struct {
    Pager* pager;
    Table* table;
    Page* page;
    int offset; // 相对于page.data
    int row_num; // 当前指向第几行
    int page_row_num; // 当前指向page中第几行
} Cursor;

DB* db_init();
void db_destory(DB* d);
Table* open_table(DB* d, char* name);
int create_table(DB* d, Ast* a);
int insert_row(DB* d, Ast* a);
void unserialize_row(void* row, RowFmt* rf, QueryResult* qr);
void* serialize_table(Table* t, int* len);
QueryResult* get_table_header(Table* t);
void destory_query_result_list(QueryResultList* qrl, int qrl_len, int qr_len);
QueryResultList* select_row(DB* d, Ast* select_ast, int* row_count, int* col_count);
static int get_where_expr_res(Table* t, void* row, Ast* where_expr);

void println_rows(QueryResultList* qrl, int qrl_len, int qr_len);

Cursor* cursor_init(Table* t);
void cursor_destory(Cursor* c);
void* cursor_value(Cursor* c);
int cursor_is_end(Cursor* c);
void cursor_next(Cursor* c);
void cursor_rewind(Cursor* c);

Page* find_free_page(Table* t, int size);
Page* get_page(Table* t, int page_num);
void* get_page_tail(Page* p);
int flush_page(Table* t, int page_num);

#endif