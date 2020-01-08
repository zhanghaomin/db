#ifndef _TABLE_H
#define _TABLE_H

#define PAGE_SIZE 4096

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

typedef struct {
    struct {
        int page_cnt;
    } header;
    int data_fd;
} Pager;

typedef struct {
    HashTable* tables;
} DB;

typedef struct {
    char* name;
    RowFmt* row_fmt;
    Pager* pager;
    DB* db;
} Table;

typedef struct {
    Pager* pager;
    Table* table;
    int page_num; // 当前指向第几页
    int page_dir_num; // 当前指向page中第几个目录项
} Cursor;

void db_destory(DB* d);
void destory_query_result_list(QueryResultList* qrl, int qrl_len, int qr_len);
int insert_row(DB* d, Ast* a);
int delete_row(DB* d, Ast* a);
int create_table(DB* d, Ast* a);
int update_table(DB* d, Ast* update);
int drop_table(DB* d, Ast* drop_table_ast);
int incr_table_free_space(Table* t, int page_num, int step);
int insert_table_free_space(Table* t, int page_num, int free_space);
DB* db_init();
Table* open_table(DB* d, char* name);
QueryResultList* select_row(DB* d, Ast* select_ast, int* row_count, int* col_count, int with_header);
int get_table_row_cnt(Table* t);
int get_query_result_val_len(QueryResultVal* qrv);
int get_page_free_space_stored(Table* t, int page_num);
int row_cmp_func(const void* p1, const void* p2);

void println_rows(QueryResultList* qrl, int qrl_len, int qr_len);

void cursor_destory(Cursor* c);
void cursor_next(Cursor* c);
int cursor_is_end(Cursor* c);
int cursor_dir_num(Cursor* c);
int cursor_value_is_deleted(Cursor* c);
int cursor_page_num(Cursor* c);
Cursor* cursor_init(Table* t);

void set_row_deleted(Table* t, int page_num, int dir_num);
int get_page_cnt(Pager* pr);
int get_page_dir_cnt(Table* t, int page_num);
int get_sizeof_dir();
int flush_pager_header(Pager* pr);
int write_row_to_page(Table* t, void* data, int size);
int row_is_delete(Table* t, int page_num, int dir_num);
int write_row_head(Table* t, int page_num, void* data, int size);
int replace_row(Table* t, int page_num, int dir_num, void* data, int len);
void* copy_row_raw_data(Table* t, int page_num, int dir_num);
Pager* init_pager(int fd);
void init_GP(int size);
void clean_page(void* data);
void destory_GP();

int calc_serialized_row_len(RowFmt* rf, QueryResult* qr);
int get_col_num_by_col_name(RowFmt* rf, char* col_name);
int get_dynamic_col_num_by_col_name(RowFmt* rf, char* col_name);
void* serialize_row(RowFmt* rf, QueryResult* qr, int* len);
QueryResultVal* unserialize_row(void* row, RowFmt* rf, char* col_name);
QueryResult* unserialize_full_row(void* row, RowFmt* rf, int* col_cnt);
QueryResult* get_table_header(Table* t);

#endif