#include "include/table.h"

static inline void* row_real_pos(Page* p, int dir_num)
{
    int offset;
    get_dir_info(p, dir_num, NULL, &offset);
    return (void*)(p->data + offset);
}

inline int get_row_len(Page* p, int dir_num)
{
    return ((RowHeader*)row_real_pos(p, dir_num))->row_len;
}

static inline int get_row_header_len()
{
    return sizeof(RowHeader);
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

int calc_serialized_row_len(RowFmt* rf, QueryResult* qr)
{
    int result = 0;
    ColFmt* cf;

    result += get_row_header_len();

    for (int i = 0; i < rf->origin_cols_count; i++) {
        cf = &rf->cols_fmt[i];

        if (cf->is_dynamic) { // 动态属性需要额外空间储存偏移量, 动态属性都是字符串
            result += sizeof(int);
            result += strlen(qr[i]->data) + 1;
        } else { // 静态属性定长
            result += cf->len;
        }

        result += sizeof(int);
    }

    return result;
}

static void translate_to_row_fmt(RowFmt* rf, QueryResult* qr)
{
    double d;
    int n;

    for (int i = 0; i < rf->origin_cols_count; i++) {
        if (rf->cols_fmt[i].type == C_CHAR || rf->cols_fmt[i].type == C_VARCHAR) {
            if (qr[i]->type == C_DOUBLE || qr[i]->type == C_INT) {
                free(qr[i]->data);
                qr[i]->data = strdup(""); // TODO
            }
        } else if (rf->cols_fmt[i].type == C_DOUBLE) {
            if (qr[i]->type == C_VARCHAR || qr[i]->type == C_CHAR) {
                d = atof(qr[i]->data);
                free(qr[i]->data);
                qr[i]->data = smalloc(sizeof(double));
                memcpy(qr[i]->data, &d, sizeof(double));
            } else if (qr[i]->type == C_INT) {
                memcpy(&n, qr[i]->data, sizeof(int));
                d = (double)n;
                free(qr[i]->data);
                qr[i]->data = smalloc(sizeof(double));
                memcpy(qr[i]->data, &d, sizeof(double));
            }
        } else {
            // int
            if (qr[i]->type == C_VARCHAR || qr[i]->type == C_CHAR) {
                n = atoi(qr[i]->data);
                free(qr[i]->data);
                qr[i]->data = smalloc(sizeof(int));
                memcpy(qr[i]->data, &n, sizeof(int));
            } else if (qr[i]->type == C_DOUBLE) {
                memcpy(&d, qr[i]->data, sizeof(double));
                n = (int)d;
                free(qr[i]->data);
                qr[i]->data = smalloc(sizeof(int));
                memcpy(qr[i]->data, &n, sizeof(int));
            }
        }
        qr[i]->type = rf->cols_fmt[i].type;
    }
}

int replace_row(Table* t, Page* p, int dir_num, RowFmt* rf, QueryResult* qr)
{
    translate_to_row_fmt(rf, qr);
    p = resize_row_space(t, t->pager, p, &dir_num, calc_serialized_row_len(rf, qr));
    return write_row_to_page(p, dir_num, rf, qr);
}

int write_row_to_page(Page* p, int dir_num, RowFmt* rf, QueryResult* qr)
{
    ColFmt* cf;
    RowHeader rh;
    void *dynamic_offset_store, *data_start, *store, *origin_store;
    int offset = 0;
    int val_len = 0;

    rh.row_len = calc_serialized_row_len(rf, qr);
    store = row_real_pos(p, dir_num);
    origin_store = smalloc(rh.row_len);
    memcpy(origin_store, store, rh.row_len);

    data_start = store + get_row_header_len(); // skip header
    dynamic_offset_store = data_start;
    store = data_start + sizeof(int) * rf->dynamic_cols_count; // skip dynamic offset list

    // 写入静态字段
    for (int i = 0; i < rf->origin_cols_count; i++) {
        cf = &rf->cols_fmt[i];

        if (!cf->is_dynamic) {
            val_len = get_query_result_val_len(qr[i]);
            memcpy(store, &val_len, sizeof(int));
            store += sizeof(int); // skip len
            memcpy(store, qr[i]->data, val_len);
            // important: 静态字段固定长度
            store += cf->len;
        }
    }

    // 写入动态字段, 修改偏移量
    for (int i = 0; i < rf->origin_cols_count; i++) {
        cf = &rf->cols_fmt[i];

        if (cf->is_dynamic) {
            val_len = get_query_result_val_len(qr[i]);
            offset = store - data_start;
            memcpy(dynamic_offset_store, &offset, sizeof(int)); // 写入动态属性的偏移量
            dynamic_offset_store += sizeof(int);
            memcpy(store, &val_len, sizeof(int)); // 写入长度
            store += sizeof(int);
            memcpy(store, qr[i]->data, val_len); // 写入数据
            store += val_len;
        }
    }

    // 写入header
    memcpy(data_start - get_row_header_len(), &rh, get_row_header_len());
    return memcmp(origin_store, data_start - get_row_header_len(), rh.row_len) != 0;
}

/*
    || RowHeader || 1st dynamic offset || 2nd dynamic offset || ... || 1st reclen | 1st static rec || 2nd reclen | 2nd static rec || ... || 1st dynamic reclen | 1st dynamic rec || 2nd dynamic reclen | 2nd dynamic rec || ...
                            |______________________________________________________________________________________________________________|
                                                  |________________________________________________________________________________________________________________________________|
     偏移量都是相对header的
*/
// 4 + 4 + 4 + 4 + 8 + 4 + 255 + 4 + 52
// 4 + 4 + (4+4) + (4+4) + (4+255) + (4+4) + (4+4) + (4 + 52)
int serialize_row(Table* t, RowFmt* rf, QueryResult* qr)
{
    Page* p;
    int dir_num;

    translate_to_row_fmt(rf, qr);
    p = reserve_new_row_space(t, t->pager, calc_serialized_row_len(rf, qr), &dir_num);
    return write_row_to_page(p, dir_num, rf, qr);
}

QueryResultVal* get_col_val(Page* p, int dir_num, RowFmt* rf, char* col_name)
{
    int col_num, dynamic_col_num, len, offset;
    void *col, *row;
    ColFmt cf;
    QueryResultVal* qrv;
    qrv = smalloc(sizeof(QueryResultVal));
    col_num = get_col_num_by_col_name(rf, col_name);
    row = row_real_pos(p, dir_num);
    row += get_row_header_len();

    if (col_num == -1) {
        return NULL;
    }

    cf = rf->cols_fmt[col_num];
    qrv->type = cf.type;

    if (cf.is_dynamic) {
        dynamic_col_num = get_dynamic_col_num_by_col_name(rf, col_name);
        memcpy(&offset, row + sizeof(int) * dynamic_col_num, sizeof(int));
        col = row + offset;
    } else {
        col = row + sizeof(int) * rf->dynamic_cols_count;
        // 跳过前面的静态字段
        for (int i = 0; i < col_num; i++) {
            if (!rf->cols_fmt[i].is_dynamic) {
                col += sizeof(int) + rf->cols_fmt[i].len;
            }
        }
    }

    memcpy(&len, col, sizeof(int)); // len
    col += sizeof(int);
    qrv->data = smalloc(len);
    memcpy(qrv->data, col, len);
    return qrv;
}