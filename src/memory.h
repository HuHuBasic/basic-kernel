/*
 * memory.h - 基础内存管理头文件
 */
#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* 堆大小: 4MB */
#define HEAP_SIZE 0x400000

/* 初始化内存管理 */
void memory_init(void);

/* 分配内存 */
void *malloc(size_t size);

/* 释放内存 */
void free(void *ptr);

/* 获取内存使用信息 */
void memory_info(uint32_t *total, uint32_t *used, uint32_t *free);

#endif /* _MEMORY_H */