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

typedef enum {
    BT_INT,
    BT_STR
} BtreeKeyType;

typedef struct _Btree {
    BtreeKeyType key_type;
    int height;
    int root;
} Btree;

typedef struct {
    struct {
        int len;
        int left; // 左指针
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

BtreeNode* get_page(int page_num)
{
    return NULL;
}

Btree* btree_init(BtreeKeyType key_type)
{
    Btree* bt;
    BtreeNode* root;
    bt = smalloc(sizeof(Btree));
    bt->height = 1;
    bt->key_type = key_type;
    bt->root = 0;
    root = get_page(0);
    root->header.rec_cnt = 0;
    root->header.fd = -1; // FIXME
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

static int space_enough(BtreeNode* node, void* key, int len)
{
    return 1;
}

static int compare(BtreeKeyType type, void* val1, void* val2, int len1, int len2)
{
    return 1;
}

static int node_key_len(BtreeNodeRow* bnr)
{
    return bnr->header.len - sizeof(BtreeNodeRow);
}

static int leaf_key_len(BtreeLeafNodeRow* blnr)
{
    return blnr->header.len - sizeof(BtreeLeafNodeRow);
}

void* btree_insert(Btree* bt, void* key, int len, int page_num, int dir_num)
{
    BtreeNode* cur;
    BtreeNodeRow* bnr;
    BtreeLeafNodeRow* blnr;
    BtreeLeafNodeRow* new_blnr;
    void* data;
    int right, new_row_len;

    cur = get_page(bt->root);
research:
    data = cur->data;

    if (cur->header.node_type == NODE) {
        for (int i = 0; i < cur->header.rec_cnt; i++) {
            bnr = data;
            if (compare(bt->key_type, key, bnr->key, len, node_key_len(bnr)) < 0) {
                // go left
                cur = get_page(bnr->header.left);
                goto research;
            }
            data += bnr->header.len;
        }
        // TODO: ?
        memcpy(&right, data, sizeof(int));
        cur = get_page(right);
        goto research;
    } else {
        for (int i = 0; i < cur->header.rec_cnt; i++) {
            blnr = data;
            if (compare(bt->key_type, key, blnr->key, len, leaf_key_len(blnr)) == 0) {
                // TODO: add into overflow page
            } else if (compare(bt->key_type, key, blnr->key, len, leaf_key_len(blnr)) < 0) {
                // 检查空间是否足够
                if (space_enough(cur, key, len)) {
                    // insert here
                    goto insert;
                } else {
                    // TODO: seperate and insert, 从父节点插入, 重新确定是插入当前节点还是新节点
                    cur = get_page(cur->header.parent);
                    goto research;
                }
            }

            data += bnr->header.len;
        }
    }

insert:
    // 后面的元素往后移动
    new_row_len = sizeof(BtreeLeafNodeRow) + len;
    memmove(data + new_row_len, data, cur->header.free_tail_offset - (data - (void*)cur->data));
    new_blnr = smalloc(new_row_len);
    new_blnr->header.dir_num = dir_num;
    new_blnr->header.len = new_row_len;
    new_blnr->header.next = -1;
    new_blnr->header.page_num = page_num;
    memcpy(new_blnr->key, key, len);
    memcpy(data, new_blnr, new_row_len);
    free(new_blnr);
    cur->header.free_space -= new_row_len;
    cur->header.free_tail_offset += new_row_len;
    cur->header.rec_cnt++;
    return NULL;
}

// 找到需要插入的叶子节点
