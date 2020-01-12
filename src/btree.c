#include "include/btree.h"
#include "include/table.h"
#include <stdlib.h>

typedef enum {
    NODE,
    LEAF,
    OVERFLOWLEAF
} BtreeNodeType;

typedef struct _BtreeNodeCommonHeader {
    int page_num; // 页号
    int rec_cnt; // 项数
    int fd;
    int is_root;
    int node_type; // 节点类型, 普通/溢出叶子/叶子
    int level; // 节点深度,叶子节点为0
    int free_space; // 剩余空间
    int free_tail_offset; // 空闲起始处, 相对于头
    int parent; // 父节点
    int next; // 后一个节点
    int prev; // 前一个节点
} BtreeNodeCommonHeader;

typedef struct _BtreeNode {
    BtreeNodeCommonHeader header;
    char data[];
} BtreeNode;

// | left | row | right |
typedef struct {
    struct {
        int len;
    } header;
    char key[];
} BtreeNodeRow;

typedef struct {
    struct {
        int len;
        int next; // 相同元素溢出的page, 相同元素放入一个新的page
        int page_num; // 元素位置
        int dir_num; // 元素位置
    } header;
    char key[];
} BtreeLeafNodeRow;

typedef struct {
    struct {
        int len;
        int page_num; // 元素位置
        int dir_num; // 元素位置
    } header;
    // char key[];
} BtreeOverFlowLeafNodeRow;

#undef PAGE_SIZE
#define PAGE_SIZE 100
static BtreeNode* map[100] = { 0 };
static int max_page_num = 0;
static void insert_into_node(Btree* bt, BtreeNode* bn, void* key, int key_len, int left, int right);

BtreeNode* get_page(int page_num)
{
    if (map[page_num] == NULL) {
        map[page_num] = smalloc(PAGE_SIZE);
    }

    if (page_num > max_page_num) {
        max_page_num = page_num;
    }

    return map[page_num];
}

BtreeNode* get_new_page()
{
    return get_page(max_page_num + 1);
}

Btree* btree_init(BtreeKeyType key_type, int fd)
{
    Btree* bt;
    BtreeNode* root;
    bt = smalloc(sizeof(Btree));
    bt->height = 1;
    bt->key_type = key_type;
    bt->root = 0;
    bt->fd = fd;
    root = get_page(0);
    root->header.rec_cnt = 0;
    root->header.fd = fd; // FIXME
    root->header.free_space = PAGE_SIZE - sizeof(BtreeNode);
    root->header.free_tail_offset = 0;
    root->header.level = 0;
    root->header.next = -1;
    root->header.prev = -1;
    root->header.node_type = LEAF;
    root->header.page_num = 0;
    root->header.parent = -1;
    root->header.is_root = 1;
    return bt;
}

void btree_destory(Btree* bt)
{
    free(bt);
}

static int node_row_len(BtreeNode* bn, int key_len)
{
    assert(bn->header.node_type == NODE || bn->header.node_type == LEAF);
    if (bn->header.node_type == NODE) {
        return sizeof(BtreeNodeRow) + key_len;
    } else {
        return sizeof(BtreeLeafNodeRow) + key_len;
    }
}

static int space_enough(BtreeNode* node, int need_space)
{
    assert(node->header.node_type == LEAF || node->header.node_type == NODE);
    return node->header.free_space >= need_space;
}

static int btree_compare(BtreeKeyType type, void* val1, void* val2, int len1, int len2)
{
    assert(type != BT_INT || len1 == len2);
    int i, j;

    if (type == BT_INT) {
        memcpy(&i, val1, sizeof(int));
        memcpy(&j, val2, sizeof(int));
        return i - j;
    }

    if (len1 < len2) {
        return -1;
    } else if (len1 > len2) {
        return 1;
    }

    return strncmp(val1, val2, len1);
}

static int node_key_len(BtreeNodeRow* bnr)
{
    return bnr->header.len - sizeof(BtreeNodeRow);
}

static int leaf_key_len(BtreeLeafNodeRow* blnr)
{
    return blnr->header.len - sizeof(BtreeLeafNodeRow);
}

static BtreeNode* make_parent_node(Btree* bt)
{
    BtreeNode* parent;
    parent = get_new_page();
    parent->header.rec_cnt = 0;
    parent->header.fd = bt->fd; // FIXME
    parent->header.free_space = PAGE_SIZE - sizeof(BtreeNode);
    parent->header.free_tail_offset = 0;
    parent->header.level = bt->height++;
    parent->header.next = -1;
    parent->header.prev = -1;
    parent->header.node_type = NODE;
    parent->header.page_num = max_page_num;
    parent->header.parent = -1;
    parent->header.is_root = 1;
    bt->root = parent->header.page_num;
    return parent;
}

static BtreeNode* add_sbling_node(BtreeNode* node)
{
    BtreeNode* new_bn;
    new_bn = get_new_page();
    new_bn->header.rec_cnt = 0;
    new_bn->header.fd = node->header.fd; // FIXME
    new_bn->header.free_space = PAGE_SIZE - sizeof(BtreeNode);
    new_bn->header.free_tail_offset = 0;
    new_bn->header.level = node->header.level;
    new_bn->header.next = node->header.next;
    new_bn->header.prev = node->header.page_num;
    new_bn->header.node_type = node->header.node_type;
    new_bn->header.page_num = max_page_num;
    new_bn->header.parent = node->header.parent;
    new_bn->header.is_root = 0;
    node->header.next = new_bn->header.page_num;
    return new_bn;
}

// 分裂页面, 返回数据应该插入的页面
static BtreeNode* split_for_space(Btree* bt, BtreeNode* node, void* key, int key_len)
{
    assert(node->header.node_type == LEAF || node->header.node_type == NODE);
    assert(!(node->header.node_type == LEAF && node->header.rec_cnt == 0 && !space_enough(node, node_row_len(node, key_len))));
    assert(!(node->header.node_type == NODE && node->header.rec_cnt == 0 && !space_enough(node, node_row_len(node, key_len) + 2 * sizeof(int))));

    BtreeNode *new_bn, *parent;
    BtreeLeafNodeRow* blnr;
    BtreeNodeRow* bnr;
    void* data;
    int move_size, release_size;
    void* parent_key;
    int parent_key_len;

    if (node->header.rec_cnt == 0) {
        return node;
    } else if (node->header.node_type == LEAF && space_enough(node, node_row_len(node, key_len))) {
        return node;
    } else if (node->header.node_type == NODE && space_enough(node, node_row_len(node, key_len) + sizeof(int))) {
        return node;
    }

    if (node->header.parent == -1) {
        // 新增父节点
        parent = make_parent_node(bt);
        node->header.parent = parent->header.page_num;
        node->header.is_root = 0;
    }

    // 新增兄弟节点
    new_bn = add_sbling_node(node);
    data = node->data;

    // 移动数据
    if (node->header.node_type == LEAF) {
        for (int i = 0; i < node->header.rec_cnt / 2; i++) {
            blnr = data;
            data += blnr->header.len;
        }

        blnr = data;
        parent_key = blnr->key;
        parent_key_len = leaf_key_len(blnr);
        release_size = node->header.free_tail_offset - (data - (void*)node->data);
    } else {
        for (int i = 0; i < node->header.rec_cnt / 2; i++) {
            bnr = data + sizeof(int);
            data += bnr->header.len + sizeof(int);
        }

        bnr = data + sizeof(int);
        parent_key = bnr->key;
        parent_key_len = node_key_len(bnr);
        release_size = node->header.free_tail_offset - (data - (void*)node->data) - sizeof(int);
    }

    move_size = node->header.free_tail_offset - (data - (void*)node->data);
    memcpy(new_bn->data, data, move_size);
    new_bn->header.free_space -= move_size;
    new_bn->header.free_tail_offset += move_size;
    new_bn->header.rec_cnt = node->header.rec_cnt - node->header.rec_cnt / 2;
    node->header.rec_cnt /= 2;
    node->header.free_space += release_size;
    node->header.free_tail_offset -= release_size;

    // left right插入父节点
    insert_into_node(bt, get_page(node->header.parent), parent_key, parent_key_len, node->header.page_num, new_bn->header.page_num);

    if (btree_compare(bt->key_type, key, parent_key, key_len, parent_key_len) < 0) {
        return get_page(node->header.page_num);
    }

    return get_page(new_bn->header.page_num);
}

static void insert_into_node(Btree* bt, BtreeNode* bn, void* key, int key_len, int left, int right)
{
    assert(bn->header.node_type == NODE);

    BtreeNodeRow *bnr, *new_bnr;
    void* data;
    int new_row_len;
    int cost_space = 0;

    bn = split_for_space(bt, bn, key, key_len);

    data = bn->data;
    new_row_len = node_row_len(bn, key_len);
    cost_space = new_row_len + sizeof(int);

    if (bn->header.rec_cnt == 0) {
        cost_space += sizeof(int); // 第一条记录会多一个right
        goto insert_to_node;
    }

    for (int i = 0; i < bn->header.rec_cnt; i++) {
        bnr = data + sizeof(int);

        if (btree_compare(bt->key_type, key, bnr->key, key_len, node_key_len(bnr)) == 0) {
            return;
        }

        if (btree_compare(bt->key_type, key, bnr->key, key_len, node_key_len(bnr)) < 0) {
            goto insert_to_node;
        }

        data += bnr->header.len + sizeof(int);
    }

insert_to_node:
    memmove(data + new_row_len + sizeof(int), data, bn->header.free_tail_offset - (data - (void*)bn->data));
    new_bnr = smalloc(new_row_len);
    new_bnr->header.len = new_row_len;
    memcpy(new_bnr->key, key, key_len);
    memcpy(data, &left, sizeof(int));
    data += sizeof(int);
    memcpy(data, new_bnr, new_row_len);
    data += new_row_len;
    memcpy(data, &right, sizeof(int));
    free(new_bnr);
    bn->header.free_space -= cost_space;
    bn->header.free_tail_offset += cost_space;
    bn->header.rec_cnt++;
    return;
}

// 将数据插入某个节点, 如果空间不够, 则分裂
static void insert_into_leaf(Btree* bt, BtreeNode* bn, void* key, int key_len, int page_num, int dir_num)
{
    assert(bn->header.node_type == LEAF);

    BtreeLeafNodeRow *blnr, *new_blnr;
    void* data;
    int new_row_len;

    bn = split_for_space(bt, bn, key, key_len);
    data = bn->data;

    if (bn->header.rec_cnt == 0) {
        goto insert_to_leaf;
    }

    for (int i = 0; i < bn->header.rec_cnt; i++) {
        blnr = data;
        if (btree_compare(bt->key_type, key, blnr->key, key_len, leaf_key_len(blnr)) == 0) {
            // TODO: add into overflow page
            assert(0);
        } else if (btree_compare(bt->key_type, key, blnr->key, key_len, leaf_key_len(blnr)) < 0) {
            goto insert_to_leaf;
        }

        data += blnr->header.len;
    }

insert_to_leaf:
    new_row_len = node_row_len(bn, key_len);
    memmove(data + new_row_len, data, bn->header.free_tail_offset - (data - (void*)bn->data));
    new_blnr = smalloc(new_row_len);
    new_blnr->header.dir_num = dir_num;
    new_blnr->header.len = new_row_len;
    new_blnr->header.next = -1;
    new_blnr->header.page_num = page_num;
    memcpy(new_blnr->key, key, key_len);
    memcpy(data, new_blnr, new_row_len);
    free(new_blnr);
    bn->header.free_space -= new_row_len;
    bn->header.free_tail_offset += new_row_len;
    bn->header.rec_cnt++;
    return;
}

void* btree_insert(Btree* bt, void* key, int key_len, int page_num, int dir_num)
{
    BtreeNode* cur;
    BtreeNodeRow* bnr;
    void* data;
    int left, right;

    cur = get_page(bt->root);
research:
    data = cur->data;

    if (cur->header.node_type == NODE) {
        for (int i = 0; i < cur->header.rec_cnt; i++) {
            memcpy(&left, data, sizeof(int));
            data += sizeof(int);
            bnr = data;
            if (btree_compare(bt->key_type, key, bnr->key, key_len, node_key_len(bnr)) < 0) {
                // go left
                cur = get_page(left);
                goto research;
            }
            data += bnr->header.len;
        }

        memcpy(&right, data, sizeof(int));
        cur = get_page(right);
        goto research;
    } else {
        insert_into_leaf(bt, cur, key, key_len, page_num, dir_num);
    }

    return key;
}

int btree_find(Btree* bt, void* key, int key_len, int* page_num, int* dir_num)
{
    BtreeNode* cur;
    BtreeNodeRow* bnr;
    BtreeLeafNodeRow* blnr;
    void* data;
    int left, right;

    cur = get_page(bt->root);
research:
    data = cur->data;

    if (cur->header.node_type == NODE) {
        for (int i = 0; i < cur->header.rec_cnt; i++) {
            memcpy(&left, data, sizeof(int));
            data += sizeof(int);
            bnr = data;
            if (btree_compare(bt->key_type, key, bnr->key, key_len, node_key_len(bnr)) < 0) {
                // go left
                cur = get_page(left);
                goto research;
            }
            data += bnr->header.len;
        }
        memcpy(&right, data, sizeof(int));
        cur = get_page(right);
        goto research;
    } else {
        for (int i = 0; i < cur->header.rec_cnt; i++) {
            blnr = data;
            if (btree_compare(bt->key_type, key, blnr->key, key_len, leaf_key_len(blnr)) == 0) {
                // TODO: check overflow page
                *page_num = blnr->header.page_num;
                *dir_num = blnr->header.dir_num;
                return 1;
            } else if (btree_compare(bt->key_type, key, blnr->key, key_len, leaf_key_len(blnr)) < 0) {
                return 0;
            }

            data += blnr->header.len;
        }
    }

    return 0;
}
