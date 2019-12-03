#ifndef UTIL_H
#define UTIL_H 1

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void sys_err(const char* format, ...);

void sys_err(const char* format, ...)
{
    va_list ap;
    char buf[1024] = { 0 };
    va_start(ap, format);
    vsprintf(buf, format, ap);
    printf("%s err: %s\n", buf, strerror(errno));
    va_end(ap);
    exit(-1);
}

#endif