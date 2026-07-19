/*
 * string.h - 基础字符串操作头文件
 */
#ifndef _STRING_H
#define _STRING_H

#include <stdint.h>
#include <stddef.h>

void *memset(void *dest, int val, size_t len);
void *memcpy(void *dest, const void *src, size_t len);
void *memmove(void *dest, const void *src, size_t len);
int   memcmp(const void *s1, const void *s2, size_t len);
size_t strlen(const char *str);
int   strcmp(const char *s1, const char *s2);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int   strncmp(const char *s1, const char *s2, size_t n);
int   isdigit(int c);
int   isalpha(int c);
int   isspace(int c);

/* 简易 atoi */
int   simple_atoi(const char *str);

/* 简易 snprintf (仅支持 %s %d %%) */
int   snprintf_simple(char *buf, int max_len, const char *fmt, ...);

#endif /* _STRING_H */