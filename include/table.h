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
// page_directory => | is_delete | offset | ...
// 不用page_directory时，一个记录的增大会导致所有记录后移，直接依赖他们偏移量的地方都需要更改（更新索引会很麻烦）
// 使用page_directory在更新时，如果空间足够，那么移动他后面的元素，外面的引用不需要修改，如果空间不够，删除原来的记录，寻找新的空间插入，外面的引用只需要更改当前记录
// 删除只标记，不删除，等剩余空间太大时再删除
typedef struct {
    struct {
        int page_num; // 页号
        int dir_cnt; // 页目录项数
    } header;
    char data[];
} Page;

typedef struct {
    Page* pages[MAX_PAGE_CNT_P_TABLE]; // 表里的page集合
} Pager;

typedef struct {
    HashTable* tables;
} DB;

typedef struct {
    char* name;
    int data_fd;
    RowFmt* row_fmt;
    Pager* pager;
    int max_page_num; // 最大叶号
    int row_count; // 表里数据总行数，不包括已删除的
    int free_map[MAX_PAGE_CNT_P_TABLE]; // 每页的空闲空间
} Table;

typedef struct {
    Pager* pager;
    Table* table;
    Page* page;
    // int row_num; // 当前指向第几行，不包括已删除的
    int page_dir_num; // 当前指向page中第几个目录项
} Cursor;

void db_destory(DB* d);
int insert_row(DB* d, Ast* a);
int delete_row(DB* d, Ast* a);
int create_table(DB* d, Ast* a);
int update_table(DB* d, Ast* update);
DB* db_init();
Table* open_table(DB* d, char* name);
QueryResultList* select_row(DB* d, Ast* select_ast, int* row_count, int* col_count, int with_header);

void destory_query_result_list(QueryResultList* qrl, int qrl_len, int qr_len);
int get_table_row_cnt(Table* t);
int get_table_max_page_num(Table* t);
int get_query_result_val_len(QueryResultVal* qrv);
int get_col_num_by_col_name(RowFmt* rf, char* col_name);
int get_dynamic_col_num_by_col_name(RowFmt* rf, char* col_name);
QueryResult* get_table_header(Table* t);

void println_rows(QueryResultList* qrl, int qrl_len, int qr_len);

void cursor_destory(Cursor* c);
void cursor_next(Cursor* c);
void cursor_rewind(Cursor* c);
int cursor_is_end(Cursor* c);
int cursor_dir_num(Cursor* c);
int cursor_value_is_deleted(Cursor* c);
Page* cursor_page(Cursor* c);
Cursor* cursor_init(Table* t);

void init_pager(Table* t);
void set_dir_info(Page* p, int dir_num, int is_delete, int row_offset);
void get_dir_info(Page* p, int dir_num, int* is_delete, int* row_offset);
int get_page_num(Page* p);
int get_page_dir_cnt(Page* p);
int flush_page(Table* t, int page_num);
Page* get_page(Table* t, int page_num);
Page* reserve_new_row_space(Table* t, int size, int* dir_num);
Page* resize_row_space(Table* t, Page* old_page, int* dir_num, int size);

void set_row_deleted(Page* p, int dir_num);
int get_row_len(Page* p, int dir_num);
int row_is_delete(Page* p, int dir_num);
int serialize_row(Table* t, RowFmt* rf, QueryResult* qr);
int replace_row(Table* t, Page* p, int dir_num, RowFmt* rf, QueryResult* qr);
QueryResultVal* get_col_val(Page* p, int dir_num, RowFmt* rf, char* col_name);

#endif