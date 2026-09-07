/* lib.c — 字符串 & 内存工具 */

#include "kernel.h"

void memcpy(void *d, const void *s, uint32 n) {
    uint8 *cd = (uint8*)d;
    const uint8 *cs = (const uint8*)s;
    for (uint32 i = 0; i < n; i++) cd[i] = cs[i];
}

void memset(void *d, uint8 v, uint32 n) {
    uint8 *cd = (uint8*)d;
    for (uint32 i = 0; i < n; i++) cd[i] = v;
}

int strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

int strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == '\0') return a[i] - b[i];
    }
    return 0;
}