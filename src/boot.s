/*
 * boot.s - 内核启动汇编代码
 * 设置 Multiboot header, GDT, 栈, 并跳转到 C 内核入口
 */

/* ---------- 常量定义 ---------- */
.set MULTIBOOT_MAGIC,        0x1BADB002
.set MULTIBOOT_ALIGN,        1 << 0
.set MULTIBOOT_MEMINFO,      1 << 1
.set MULTIBOOT_VIDEO,        1 << 2
.set MULTIBOOT_FLAGS,        MULTIBOOT_ALIGN | MULTIBOOT_MEMINFO | MULTIBOOT_VIDEO
.set MULTIBOOT_CHECKSUM,     -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

.set STACK_SIZE,             0x4000        /* 16KB 栈 */

/* ---------- Multiboot Header ---------- */
.section .multiboot
.align 4
.long MULTIBOOT_MAGIC
.long MULTIBOOT_FLAGS
.long MULTIBOOT_CHECKSUM

/* 视频信息请求 (可选, 用于设置帧缓冲) */
.long 0, 0, 0, 0, 0          /* header_addr, load_addr, load_end_addr, bss_end_addr, entry_addr */
.long 0                       /* 图形模式: 0 = 线性图形 */
.long 1024, 768, 32           /* 宽度, 高度, 深度 */

/* ---------- 代码段 ---------- */
.section .text
.global _start
.type _start, @function

_start:
    /* 保存 Multiboot 信息 (ebx 包含 multiboot 结构体指针) */
    movl %eax, multiboot_magic
    movl %ebx, multiboot_info

    /* 设置栈指针 */
    movl $stack_top, %esp

    /* 重置 EFLAGS */
    pushl $0
    popf

    /* 调用全局构造函数 */
    /* call _init */

    /* 跳转到 C 内核入口 */
    call kernel_main

    /* 如果 kernel_main 返回, 进入挂起循环 */
    cli
hang:
    hlt
    jmp hang

.size _start, . - _start

/* ---------- GDT (全局描述符表) ---------- */
.section .data
.align 16
gdt_start:
    /* 空描述符 (必须) */
    .long 0x00000000
    .long 0x00000000

    /* 内核代码段: 基址=0, 界限=4GB, DPL=0, 可执行+可读 */
    .long 0x0000FFFF      /* 低 32 位: 段界限[15:0], 基址[15:0] */
    .long 0x00CF9A00      /* 高 32 位: 基址[23:16], G=1, D/B=1, 界限[19:16], P=1, DPL=0, S=1, Type=1010 */

    /* 内核数据段: 基址=0, 界限=4GB, DPL=0, 可写+可读 */
    .long 0x0000FFFF
    .long 0x00CF9200      /* Type=0010 */

    /* 用户代码段: 基址=0, 界限=4GB, DPL=3, 可执行+可读 */
    .long 0x0000FFFF
    .long 0x00CFFA00      /* DPL=3 */

    /* 用户数据段: 基址=0, 界限=4GB, DPL=3, 可写+可读 */
    .long 0x0000FFFF
    .long 0x00CFF200      /* DPL=3 */
gdt_end:

.global gdt_descriptor
gdt_descriptor:
    .word gdt_end - gdt_start - 1  /* 界限 */
    .long gdt_start                 /* 基址 */

/* ---------- 加载 GDT 的函数 ---------- */
.section .text
.global gdt_flush
.type gdt_flush, @function
gdt_flush:
    movl 4(%esp), %eax          /* 获取 gdt_descriptor 指针 */
    lgdt (%eax)

    /* 远跳转刷新 CS 选择子: 0x08 = 内核代码段 */
    ljmp $0x08, $flush_cs

flush_cs:
    /* 设置数据段寄存器: 0x10 = 内核数据段 */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ret

.size gdt_flush, . - gdt_flush

/* ---------- BSS 区 (栈) ---------- */
.section .bss
.align 16
stack_bottom:
    .skip STACK_SIZE
stack_top:

/* Multiboot 信息保存 */
.global multiboot_magic
.global multiboot_info
multiboot_magic:
    .long 0
multiboot_info:
    .long 0
