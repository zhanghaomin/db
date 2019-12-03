#include "btree.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int btree_find_in_leaf(btree_node* node, int id)
{
    assert(node->is_leaf);
    int scan_rec_cnt = 0;
    void* pos;

    pos = (void*)node + sizeof(btree_node);
find:
    if (scan_rec_cnt == node->rec_num) { // 到底了
        return 0;
    }

    db_row r;
    memcpy(&r, pos, sizeof(r));

    if (r.id > id) {
        return 0;
    } else if (r.id < id) { // 右移
        pos += get_row_size();
        scan_rec_cnt++;
        goto find;
    }

    printf("btree_node size: %d\n", sizeof(btree_node));
    return sizeof(btree_node) + scan_rec_cnt * get_row_size();
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
        return btree_find_in_leaf(current_node, id);
    }

    pos = (void*)current_node + sizeof(btree_node);
find_in_old_node:
    memcpy(&left, pos, sizeof(int));

    // 到底了
    if (scan_rec_cnt == current_node->rec_num) {
        current_page_num = left;
        goto find_in_new_node;
    }

    memcpy(&current_id, pos + sizeof(int), sizeof(int));
    memcpy(&right, pos + sizeof(int) + sizeof(int), sizeof(int));

    if (id < current_id) { // 进入子节点
        current_page_num = left;
        goto find_in_new_node;
    } else if (id > current_id) { // 右移动
        pos += sizeof(int) + row_size;
        scan_rec_cnt++;
        goto find_in_old_node;
    } else { // 右边大于等于,进入叶子节点
        current_page_num = right;
        goto find_in_new_node;
    }
}

void btree_insert(db_table* t, db_row* r)
{
    btree_node* node;
    void* current;
    node = (btree_node*)get_page(t->pager, t->primary_data->last_page);
    node->is_leaf = 1;
    current = node->rec_num * get_row_size() + sizeof(btree_node) + (void*)node;
    printf("current: %p\n", current);
    printf("row size: %ld\n", get_row_size());
    memcpy(current, r, sizeof(db_row));
    node->rec_num++;
    return;
}

int get_row_size()
{
    return sizeof(db_row);
}