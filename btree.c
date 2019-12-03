#include "btree.h"
#include <stdlib.h>

db_row* get_row_by_row_pos(void* page, int pos)
{
}

int get_right_node(db_row* r)
{
}

int get_left_node(db_row* r)
{
}

db_row* btree_find(db_table* t, int id)
{
    btree_node* current_node;
    page_directory_entry* current;
    current_node = (btree_node*)get_page(t->pager, t->primary_data->root);
    int current_id, current_row_pos;

find_row:
    for (int i = 0; i < current_node->rec_num; i++) {
        current_id = current_node->page_directory[i].id;
        current_row_pos = current_node->page_directory[i].row_pos;

        if (i == 0) {
        }

        if (i == current_node->rec_num - 1) {
            /* code */
        }

        

        if (i == 0 && current_node->page_directory[i].id > id) {
            current_node = (btree_node*)get_page(t->pager, get_left_node(get_row_by_row_pos((void*)current_node, current_node->page_directory[i].row_pos)));
            goto find_row;
        } else if (current_node->page_directory[i].id < id) {
            continue;
        } else if (current_node->page_directory[i].id == id) {
            return get_row_by_row_pos((void*)current_node, current_node->page_directory[i].row_pos);
        } else if (current_node->page_directory[i].id > id) {
            if (i == current_node->rec_num - 1) {
                current_node = (btree_node*)get_page(t->pager, get_right_node(get_row_by_row_pos((void*)current_node, current_node->page_directory[i].row_pos)));
                goto find_row;
            }

            return NULL;
        }
    }

    return 0;
}