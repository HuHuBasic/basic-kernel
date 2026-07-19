/*
 * linker.ld - 内核链接脚本
 * 将内核加载到 1MB 物理地址
 */
ENTRY(_start)

SECTIONS
{
    /* 内核加载到 1MB 处 */
    . = 1M;

    /* Multiboot header 必须在 8KB 以内 */
    .multiboot BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
    }

    /* 代码段 */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.text)
    }

    /* 只读数据 */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    /* 已初始化数据 */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* 未初始化数据 (BSS) */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    /* 内核结束标记 */
    _kernel_end = .;
}