#include "include/btree.h"
#include "include/util.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc UNUSED, char const* argv[] UNUSED)
{
    Btree* bt;
    bt = btree_init(BT_STR, -1);
    char* key1 = "fffffffff";
    char* key2 = "ddddddddd";
    char* key3 = "ccccccccc";
    char* key4 = "aaaaaaaaa";
    char* key5 = "eeeeeeeee";
    char* key6 = "bbbbbbbbb";
    assert(bt != NULL);
    assert(bt->height == 1);
    assert(bt->key_type == BT_STR);
    assert(bt->root == 0);
    assert(btree_insert(bt, key2, 10, 1, 1) == key2); // d
    assert(btree_insert(bt, key1, 10, 1, 2) == key1); // f
    assert(btree_insert(bt, key3, 10, 1, 3) == key3); // c
    assert(btree_insert(bt, key4, 10, 1, 4) == key4); // a
    assert(btree_insert(bt, key6, 10, 1, 5) == key6); // b
    assert(btree_insert(bt, key5, 10, 1, 6) == key5); // e

    int page_num, dir_num;
    assert(btree_find(bt, "a", 2, &page_num, &dir_num) == 0);
    assert(btree_find(bt, key1, 10, &page_num, &dir_num) == 1);
    assert(page_num == 1 && dir_num == 2);
    assert(btree_find(bt, key2, 10, &page_num, &dir_num) == 1);
    assert(page_num == 1 && dir_num == 1);
    assert(btree_find(bt, key3, 10, &page_num, &dir_num) == 1);
    assert(page_num == 1 && dir_num == 3);
    assert(btree_find(bt, key4, 10, &page_num, &dir_num) == 1);
    assert(page_num == 1 && dir_num == 4);
    assert(btree_find(bt, key5, 10, &page_num, &dir_num) == 1);
    assert(page_num == 1 && dir_num == 6);
    assert(btree_find(bt, key6, 10, &page_num, &dir_num) == 1);
    assert(page_num == 1 && dir_num == 5);
    btree_destory(bt);
    printf("btree pass\n");
    return 0;
}
