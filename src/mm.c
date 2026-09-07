/* mm.c — 物理页分配器 (位图算法) */

#include "kernel.h"

#define PMM_BITMAP_BASE 0x200000    /* 位图放在 2MB 处 */
#define PMM_BLOCK_SIZE  4096
#define PMM_MAX_BLOCKS  262144       /* 最多管理 1GB */

static uint32 *bitmap = (uint32*)PMM_BITMAP_BASE;
static uint32 total_blocks = 0;
static uint32 free_blocks = 0;
static uint32 mem_end = 0;

static inline void bitmap_set(uint32 i)   { bitmap[i / 32] &= ~(1 << (i % 32)); }
static inline void bitmap_clear(uint32 i) { bitmap[i / 32] |= (1 << (i % 32)); }
static inline int  bitmap_test(uint32 i)  { return (bitmap[i / 32] & (1 << (i % 32))) == 0; }

/* Multiboot memory map 结构 */
#pragma pack(1)
typedef struct {
    uint32 size;
    uint32 base_low;
    uint32 base_high;
    uint32 len_low;
    uint32 len_high;
    uint32 type;
} mmap_entry_t;
#pragma pack()

void mm_init(uint32 mem_upper_kb) {
    uint32 total_mem = mem_upper_kb + 1024; /* 加上低端 1MB */
    total_blocks = total_mem * 1024 / PMM_BLOCK_SIZE;
    if (total_blocks > PMM_MAX_BLOCKS) total_blocks = PMM_MAX_BLOCKS;

    uint32 bitmap_words = (total_blocks + 31) / 32;
    for (uint32 i = 0; i < bitmap_words; i++)
        bitmap[i] = 0xFFFFFFFF;  /* 全部标记为已用 */

    /* 标记 0-1MB 为已用 (内核空间) */
    uint32 kernel_end_block = (PMM_BITMAP_BASE + bitmap_words * 4 + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;

    /* 释放从内核结束到内存末尾的所有页 */
    for (uint32 i = kernel_end_block; i < total_blocks; i++)
        bitmap_clear(i);

    free_blocks = total_blocks - kernel_end_block;
    mem_end = total_mem * 1024;

    vga_write("  [OK] Physical Memory: ");
    vga_dec(total_mem / 1024);
    vga_write(" MB (");
    vga_dec(free_blocks * 4 / 1024);
    vga_write(" MB free)\n");
}

void* pmm_alloc_page(void) {
    for (uint32 i = 0; i < total_blocks; i++) {
        if (bitmap_test(i)) {
            bitmap_set(i);
            free_blocks--;
            return (void*)(i * PMM_BLOCK_SIZE);
        }
    }
    return NULL;
}

void pmm_free_page(void *addr) {
    uint32 block = (uint32)addr / PMM_BLOCK_SIZE;
    if (block < total_blocks && !bitmap_test(block)) {
        bitmap_clear(block);
        free_blocks++;
    }
}

uint32 mm_get_free(void) { return free_blocks * 4 / 1024; }