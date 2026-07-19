/*
 * string.c - 基础字符串操作实现
 */
#include "string.h"

void *memset(void *dest, int val, size_t len)
{
    unsigned char *ptr = (unsigned char *)dest;
    for (size_t i = 0; i < len; i++)
        ptr[i] = (unsigned char)val;
    return dest;
}

void *memcpy(void *dest, const void *src, size_t len)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < len; i++)
        d[i] = s[i];
    return dest;
}

void *memmove(void *dest, const void *src, size_t len)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        for (size_t i = 0; i < len; i++)
            d[i] = s[i];
    } else if (d > s) {
        for (size_t i = len; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t len)
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    for (size_t i = 0; i < len; i++) {
        if (p1[i] != p2[i])
            return p1[i] - p2[i];
    }
    return 0;
}

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++))
        ;
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i])
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        if (s1[i] == '\0')
            return 0;
    }
    return 0;
}

int isdigit(int c)
{
    return (c >= '0' && c <= '9');
}

int isalpha(int c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

int isspace(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f');
}

int simple_atoi(const char *str)
{
    int result = 0;
    int sign = 1;

    while (isspace(*str)) str++;

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    while (isdigit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }

    return sign * result;
}

int snprintf_simple(char *buf, int max_len, const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    int pos = 0;
    const char *p = fmt;

    while (*p && pos < max_len - 1) {
        if (*p == '%' && *(p + 1)) {
            p++;
            switch (*p) {
                case 's': {
                    const char *s = __builtin_va_arg(args, const char *);
                    if (s) {
                        while (*s && pos < max_len - 1)
                            buf[pos++] = *s++;
                    } else {
                        const char *null_str = "(null)";
                        while (*null_str && pos < max_len - 1)
                            buf[pos++] = *null_str++;
                    }
                    break;
                }
                case 'd': {
                    int d = __builtin_va_arg(args, int);
                    if (d < 0) {
                        buf[pos++] = '-';
                        d = -d;
                    }
                    if (d == 0) {
                        buf[pos++] = '0';
                    } else {
                        char tmp[12];
                        int tpos = 0;
                        while (d > 0 && pos < max_len - 1) {
                            tmp[tpos++] = '0' + (d % 10);
                            d /= 10;
                        }
                        for (int i = tpos - 1; i >= 0 && pos < max_len - 1; i--)
                            buf[pos++] = tmp[i];
                    }
                    break;
                }
                case '%':
                    buf[pos++] = '%';
                    break;
                default:
                    buf[pos++] = '%';
                    buf[pos++] = *p;
                    break;
            }
            p++;
        } else {
            buf[pos++] = *p++;
        }
    }

    buf[pos] = '\0';
    __builtin_va_end(args);
    return pos;
}