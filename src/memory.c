/*
 * memory.c - 基础内存管理实现
 * 使用简单的首次适配 (First-Fit) 算法
 */
#include "memory.h"
#include "vga.h"
#include "string.h"

/* 内存块头部 */
typedef struct block_header {
    size_t size;              /* 块大小 (包括头部) */
    int    free;              /* 是否空闲 */
    struct block_header *next; /* 下一个块 */
} block_header_t;

/* 堆空间 */
static uint8_t heap[HEAP_SIZE] __attribute__((aligned(16)));
static block_header_t *heap_start = NULL;
static uint32_t heap_used = 0;

#define BLOCK_MIN_SIZE (sizeof(block_header_t) + 16)

static block_header_t *find_free_block(block_header_t **last, size_t size)
{
    block_header_t *current = heap_start;
    while (current && !(current->free && current->size >= size)) {
        *last = current;
        current = current->next;
    }
    return current;
}

static block_header_t *request_space(block_header_t *last, size_t size)
{
    block_header_t *block;
    block = (block_header_t *)((uint8_t *)last + last->size);

    if ((uintptr_t)block + size > (uintptr_t)heap + HEAP_SIZE) {
        return NULL; /* 堆空间不足 */
    }

    block->size = size;
    block->free = 0;
    block->next = NULL;
    last->next = block;

    heap_used += size;
    return block;
}

void memory_init(void)
{
    heap_start = (block_header_t *)heap;
    heap_start->size = HEAP_SIZE;
    heap_start->free = 1;
    heap_start->next = NULL;
    heap_used = sizeof(block_header_t);
}

void *malloc(size_t size)
{
    if (size == 0) return NULL;
    if (heap_start == NULL) memory_init();

    /* 对齐到 8 字节 */
    size = (size + 7) & ~7;
    size += sizeof(block_header_t);
    if (size < BLOCK_MIN_SIZE) size = BLOCK_MIN_SIZE;

    block_header_t *last = heap_start;
    block_header_t *block = find_free_block(&last, size);

    if (block == NULL) {
        /* 没有合适块, 尝试扩展堆 */
        block = request_space(last, size);
        if (block == NULL) {
            return NULL;
        }
    } else {
        /* 分裂块 (如果剩余空间足够) */
        if (block->size - size >= BLOCK_MIN_SIZE) {
            block_header_t *new_block = (block_header_t *)((uint8_t *)block + size);
            new_block->size = block->size - size;
            new_block->free = 1;
            new_block->next = block->next;
            block->size = size;
            block->next = new_block;
        }
        block->free = 0;
        heap_used += block->size;
    }

    return (void *)((uint8_t *)block + sizeof(block_header_t));
}

void free(void *ptr)
{
    if (ptr == NULL) return;

    block_header_t *block = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    block->free = 1;
    heap_used -= block->size;

    /* 合并相邻空闲块 */
    block_header_t *current = heap_start;
    while (current && current->next) {
        if (current->free && current->next->free) {
            current->size += current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

void memory_info(uint32_t *total, uint32_t *used, uint32_t *free)
{
    *total = HEAP_SIZE;
    *used = heap_used;
    *free = HEAP_SIZE - heap_used;
}