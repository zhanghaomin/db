#ifndef _UTIL_H
#define _UTIL_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define sys_err(notice) _sys_err(notice, __FILE__, __LINE__)

extern void _sys_err(char* notice, char* file, int lineno);
extern int open_file(char* name);
extern int strindex(char* str, char c);

#endif