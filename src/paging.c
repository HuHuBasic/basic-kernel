/* paging.c — 分页 + 内核堆分配器 */

#include "kernel.h"

#define PAGE_DIR_BASE  0x300000
#define PAGE_TBL_BASE  0x301000
#define KHEAP_START    0xD0000000
#define KHEAP_INITIAL  0x400000   /* 4MB 初始堆 */

static uint32 *page_dir = (uint32*)PAGE_DIR_BASE;
static uint32 *page_tbl0 = (uint32*)PAGE_TBL_BASE;  /* 第一个页表: 0-4MB */
static uint32 *page_tbl1 = (uint32*)(PAGE_TBL_BASE + 0x1000); /* 第二个: 4MB-8MB */

/* 内核堆 — 简易链表分配器 */
typedef struct heap_block {
    uint32 size;       /* 含头大小 */
    uint32 free;
    struct heap_block *next;
    struct heap_block *prev;
} heap_block_t;

static heap_block_t *heap_start = NULL;
static uint32 heap_cur = 0, heap_max = 0;

void paging_init(void) {
    /* 清空页目录和页表 */
    for (int i = 0; i < 1024; i++) page_dir[i] = 0;
    for (int i = 0; i < 1024; i++) page_tbl0[i] = 0;
    for (int i = 0; i < 1024; i++) page_tbl1[i] = 0;

    /* 0-4MB: 第一张页表，identity mapping */
    for (int i = 0; i < 1024; i++)
        page_tbl0[i] = (i * 0x1000) | 3;  /* Present + RW */

    /* 4MB-8MB: 第二张页表 */
    for (int i = 0; i < 1024; i++)
        page_tbl1[i] = (0x400000 + i * 0x1000) | 3;

    page_dir[0] = (uint32)page_tbl0 | 3;
    page_dir[1] = (uint32)page_tbl1 | 3;

    /* 加载页目录 */
    __asm__ volatile ("mov %0, %%cr3" : : "r"(page_dir));

    /* 启用分页 */
    uint32 cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    vga_write("  [OK] Paging enabled (identity mapped first 8MB)\n");

    /* 初始化内核堆 */
    heap_start = (heap_block_t*)KHEAP_START;
    heap_start->size = KHEAP_INITIAL;
    heap_start->free = 1;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    heap_cur = KHEAP_START + KHEAP_INITIAL;
    heap_max = KHEAP_START + KHEAP_INITIAL;

    vga_write("  [OK] Kernel heap: ");
    vga_dec(KHEAP_INITIAL / 1024);
    vga_write(" KB at ");
    vga_hex(KHEAP_START);
    vga_write("\n");
}

void* kmalloc(uint32 size) {
    if (size == 0) return NULL;
    size = (size + 15) & ~15;   /* 16 字节对齐 */

    heap_block_t *cur = heap_start;
    while (cur) {
        if (cur->free && cur->size >= size + sizeof(heap_block_t)) {
            uint32 remaining = cur->size - size - sizeof(heap_block_t);
            if (remaining > sizeof(heap_block_t) + 16) {
                /* 分裂 */
                heap_block_t *new_block = (heap_block_t*)((uint32)cur + sizeof(heap_block_t) + size);
                new_block->size = remaining;
                new_block->free = 1;
                new_block->next = cur->next;
                new_block->prev = cur;
                if (cur->next) cur->next->prev = new_block;
                cur->next = new_block;
                cur->size = size + sizeof(heap_block_t);
            }
            cur->free = 0;
            return (void*)((uint32)cur + sizeof(heap_block_t));
        }
        cur = cur->next;
    }

    /* 扩展堆 */
    uint32 need = size + sizeof(heap_block_t);
    if (heap_cur + need > heap_max) {
        uint32 pages = (need + 0xFFF) / 0x1000;
        for (uint32 i = 0; i < pages; i++) {
            void *p = pmm_alloc_page();
            if (!p) return NULL;
            /* 映射到堆空间 */
            uint32 vaddr = heap_cur + i * 0x1000;
            uint32 pd_idx = vaddr >> 22;
            uint32 pt_idx = (vaddr >> 12) & 0x3FF;
            /* 按需分配页表 */
            if (!(page_dir[pd_idx] & 1)) {
                void *pt = pmm_alloc_page();
                page_dir[pd_idx] = (uint32)pt | 3;
                for (int j = 0; j < 1024; j++)
                    ((uint32*)pt)[j] = 0;
            }
            uint32 *pt = (uint32*)(page_dir[pd_idx] & ~0xFFF);
            pt[pt_idx] = (uint32)p | 3;
        }
        heap_max += pages * 0x1000;
        heap_cur = heap_max;
    }

    heap_block_t *blk = (heap_block_t*)heap_cur;
    blk->size = need;
    blk->free = 0;
    blk->next = NULL;
    blk->prev = NULL;

    /* 链接到链表末尾 */
    if (heap_start) {
        heap_block_t *last = heap_start;
        while (last->next) last = last->next;
        last->next = blk;
        blk->prev = last;
    }

    heap_cur += need;
    return (void*)((uint32)blk + sizeof(heap_block_t));
}

void kfree(void *ptr) {
    if (!ptr) return;
    heap_block_t *blk = (heap_block_t*)((uint32)ptr - sizeof(heap_block_t));
    blk->free = 1;

    /* 合并相邻空闲块 */
    if (blk->next && blk->next->free) {
        blk->size += blk->next->size;
        blk->next = blk->next->next;
        if (blk->next) blk->next->prev = blk;
    }
    if (blk->prev && blk->prev->free) {
        blk->prev->size += blk->size;
        blk->prev->next = blk->next;
        if (blk->next) blk->next->prev = blk->prev;
    }
}