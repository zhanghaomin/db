#include "include/lru_list.h"
#include "include/util.h"
#include <assert.h>

void arr_to_str(char** str_arr, char* res, int len)
{
    for (int i = 0; i < len; i++) {
        strcat(res, " ");
        strcat(res, str_arr[i]);
    }
}

void assert_eq_str_arr(char** str_arr1, char** str_arr2, int len)
{
    char s1[1024] = { 0 };
    char s2[1024] = { 0 };
    arr_to_str(str_arr1, s1, len);
    arr_to_str(str_arr2, s2, len);
    if (strcmp(s1, s2) != 0) {
        printf("expect [ %s ] but given [ %s ]\n", s1, s2);
        exit(-1);
    }
}

int main(int argc UNUSED, char const* argv[] UNUSED)
{
    LruList* l;
    char* s1 = "111";
    char* s2 = "222";
    char* s3 = "333";
    char* s4 = "444";
    char** res;
    int len;

    l = lru_list_init(3, NULL, NULL);
    assert(l != NULL);
    assert(strcmp(lru_list_add(l, "key1", s1), s1) == 0);
    assert(lru_list_add(l, "key1", s1) == NULL);
    assert(strcmp(lru_list_get(l, "key1"), s1) == 0);
    assert(strcmp(lru_list_add(l, "key2", s2), s2) == 0);
    assert(strcmp(lru_list_add(l, "key3", s3), s3) == 0);
    assert(strcmp(lru_list_add(l, "key4", s4), s4) == 0); // 4 3 2
    res = (char**)lru_get_all(l, &len);
    char* expect1[] = { "444", "333", "222" };
    assert(len == 3);
    assert_eq_str_arr(expect1, res, len);

    assert(strcmp(lru_list_head(l), s4) == 0);
    assert(strcmp(lru_list_tail(l), s2) == 0);
    assert(lru_list_get(l, "key1") == NULL);

    assert(strcmp(lru_list_get(l, "key3"), s3) == 0); // 3 4 2
    assert(strcmp(lru_list_head(l), s3) == 0);
    assert(strcmp(lru_list_tail(l), s2) == 0);
    // assert(len == 3);
    res = (char**)lru_get_all(l, &len);
    assert(len == 3);
    char* expect2[] = { "333", "444", "222" };
    assert_eq_str_arr(expect2, res, len);

    assert(strcmp(lru_list_add(l, "key1", s1), s1) == 0); // 1 3 4
    assert(strcmp(lru_list_head(l), s1) == 0);
    assert(strcmp(lru_list_tail(l), s4) == 0);
    assert(lru_list_get(l, "key2") == NULL);
    res = (char**)lru_get_all(l, &len);
    assert(len == 3);
    char* expect3[] = { "111", "333", "444" };
    assert_eq_str_arr(expect3, res, len);

    // printf("%s %s\n", lru_list_head(l), lru_list_tail(l));
    assert(strcmp(lru_list_get(l, "key4"), s4) == 0); // 4 1 3
    // printf("%s %s\n", lru_list_head(l), lru_list_tail(l));
    assert(strcmp(lru_list_head(l), s4) == 0);
    assert(strcmp(lru_list_tail(l), s3) == 0);
    res = (char**)lru_get_all(l, &len);
    assert(len == 3);
    char* expect4[] = { "444", "111", "333" };
    assert_eq_str_arr(expect4, res, len);

    assert(strcmp(lru_list_add(l, "key2", s2), s2) == 0); // 2 4 1
    assert(strcmp(lru_list_head(l), s2) == 0);
    assert(strcmp(lru_list_tail(l), s1) == 0);
    assert(lru_list_get(l, "key3") == NULL);
    res = (char**)lru_get_all(l, &len);
    assert(len == 3);
    char* expect5[] = { "222", "444", "111" };
    assert_eq_str_arr(expect5, res, len);

    l = lru_list_init(1, NULL, NULL);
    assert(strcmp(lru_list_add(l, "key1", s1), s1) == 0);
    assert(strcmp(lru_list_add(l, "key2", s2), s2) == 0);
    assert(lru_list_get(l, "key1") == NULL);
    assert(strcmp(lru_list_get(l, "key2"), s2) == 0);
    res = (char**)lru_get_all(l, &len);
    assert(len == 1);
    char* expect6[] = { "222" };
    assert_eq_str_arr(expect6, res, len);

    printf("lru test pass\n");
    return 0;
}
