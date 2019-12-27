#include "include/table.h"

inline int get_row_len(void* row_start)
{
    return ((RowHeader*)row_start)->row_len;
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

inline void* row_real_pos(Page* p, int dir_num)
{
    int offset;
    get_dir_info(p, dir_num, NULL, &offset);
    return (void*)(p->data + offset);
}

int cacl_serialized_row_len(RowFmt* rf, QueryResult* qr)
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

/*
    || RowHeader || 1st dynamic offset || 2nd dynamic offset || ... || 1st reclen | 1st static rec || 2nd reclen | 2nd static rec || ... || 1st dynamic reclen | 1st dynamic rec || 2nd dynamic reclen | 2nd dynamic rec || ...
                            |______________________________________________________________________________________________________________|
                                                  |________________________________________________________________________________________________________________________________|
     偏移量都是相对header的
*/
// 4 + 4 + 4 + 4 + 8 + 4 + 255 + 4 + 52
// 4 + 4 + (4+4) + (4+4) + (4+255) + (4+4) + (4+4) + (4 + 52)
int serialize_row(void* store, RowFmt* rf, QueryResult* qr)
{
    ColFmt* cf;
    RowHeader rh;
    rh.row_len = 0;
    void *dynamic_offset_store, *data_start;
    int offset = 0;
    int val_len = 0;

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
            rh.row_len += sizeof(int);
            rh.row_len += cf->len;
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
            rh.row_len += 2 * sizeof(int);
            rh.row_len += val_len;
        }
    }

    // 长度加上自己
    rh.row_len += get_row_header_len();
    // 写入header
    memcpy(data_start - get_row_header_len(), &rh, get_row_header_len());
    return 0;
}

QueryResultVal* get_col_val(void* row, RowFmt* rf, char* col_name)
{
    int col_num, dynamic_col_num, len, offset;
    void* col;
    ColFmt cf;
    QueryResultVal* qrv;
    qrv = smalloc(sizeof(QueryResultVal));
    col_num = get_col_num_by_col_name(rf, col_name);
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