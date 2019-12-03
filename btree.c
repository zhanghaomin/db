#include "btree.h"
#include <stdlib.h>
#include <string.h>

db_row* get_row(void* page, int pos)
{
}

int get_right(db_row* r)
{
}

int get_left(db_row* r)
{
}

int btree_find(db_table* t, int id)
{
    btree_node* current_node;
    void* pos;
    int row_size;
    int current_id, left, right;
    int current_page_num;
    int scan_rec_cnt;
    current_page_num = t->primary_data->root;

find_in_new_node:
    scan_rec_cnt = 0;
    current_node = (btree_node*)get_page(t->pager, current_page_num);

    if (current_node->is_leaf) {
        row_size = get_row_size();
    } else {
        row_size = sizeof(int);
    }

    pos = (void*)current_node + sizeof(btree_node);
find_in_old_node:
    memcpy(&left, pos, sizeof(int));

    // 到底了
    if (scan_rec_cnt == current_node->rec_num) {
        current_page_num = left;
        goto find_in_new_node;
        goto find_in_new_node;
    }

    memcpy(&current_id, pos + sizeof(int), sizeof(int));
    memcpy(&right, pos + sizeof(int) + row_size, sizeof(int));

    // 进入子节点
    if (id < current_id) {
        current_page_num = left;
        goto find_in_new_node;
    }

    // 右移
    if (id > current_id) {
        pos += sizeof(int) + row_size;
        scan_rec_cnt++;
        goto find_in_old_node;
    }

    // 是叶子，直接取值，不是的话，要进入叶子节点
    if (id == current_id) {
        if (current_node->is_leaf) {
            return (int)(pos + sizeof(int));
        }

        current_page_num = left;
        goto find_in_new_node;
    }
}