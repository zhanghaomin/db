#include "util.h"

void _sys_err(char* notice, char* file, int lineno)
{
    printf("system error occured in file %s at line %d, %s err: %s.\n", file, lineno, notice, strerror(errno));
    exit(-1);
}

int strindex(char* s, char c)
{
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] == c) {
            return i;
        }
    }
    return -1;
}

int open_file(char* name)
{
    int pos = 0;
    char str[1024] = { 0 };
    char* tmp;
    tmp = name;

    while ((pos = strindex(tmp, '/')) != -1) {
        strncat(str, tmp, pos + 1);
        tmp += pos + 1;

        if (mkdir(str, 0755) == -1) {
            if (errno == EEXIST) {
                errno = 0;
                continue;
            }
            return -1;
        }
    }

    return open(name, O_RDWR | O_CREAT, 0644);
}

void* _smalloc(size_t size, char* file, int lineno)
{
    void* data;
    data = malloc(size);

    if (data == NULL) {
        _sys_err("malloc", file, lineno);
    }

    return data;
}

void* _scalloc(size_t size, size_t repeat, char* file, int lineno)
{
    void* data;
    data = calloc(size, repeat);

    if (data == NULL) {
        _sys_err("calloc", file, lineno);
    }

    return data;
}

void strreverse(char* begin, char* end)
{
    char aux;
    while (end > begin)
        aux = *end, *end-- = *begin, *begin++ = aux;
}

void itoa(int value, char* str, int base)
{
    static char num[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char* wstr = str;
    int sign;
    div_t res;
    // Validate base

    if (base < 2 || base > 35) {
        *wstr = '\0';
        return;
    }

    // Take care of sign
    if ((sign = value) < 0)
        value = -value;

    // Conversion. Number is reversed.
    do {
        res = div(value, base);
        *wstr++ = num[res.rem];
    } while ((value = res.quot));

    if (sign < 0)
        *wstr++ = '-';
    *wstr = '\0';

    // Reverse string
    strreverse(str, wstr - 1);
}