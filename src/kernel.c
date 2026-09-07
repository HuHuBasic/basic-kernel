/* kernel.c — Basic Kernel v2.0 主入口 */

#include "kernel.h"

extern uint32 __bss_end;  /* 链接脚本中定义的 BSS 结束 */

void kernel_main(uint32 magic, uint32 mb_info) {
    (void)magic;

    vga_clear();
    vga_set_color(LGRAY, BLACK);

    /* 启动画面 */
    vga_write_color("\n", GREEN);
    vga_write_color("  ____           _      _      _  __                    _   __\n", GREEN);
    vga_write_color(" |  _ \\         (_)    | |    | |/ /                   | | / /\n", GREEN);
    vga_write_color(" | |_) |_ __ ___ _  ___| | __ | ' / ___ _ __ _ __   ___| |/ /_\n", GREEN);
    vga_write_color(" |  _ <| '__/ __| |/ __| |/ / |  < / _ \\ '__| '_ \\ / _ \\ | '_ \\ \n", GREEN);
    vga_write_color(" | |_) | | | (__| | (__|   <  | . \\  __/ |  | | | |  __/ | (_) |\n", GREEN);
    vga_write_color(" |____/|_|  \\___|_|\\___|_|\\_\\ |_|\\_\\___|_|  |_| |_|\\___|_|\\___/\n", GREEN);
    vga_write_color("\n  Basic Kernel v2.0 — 32-bit x86 Operating System\n", GREEN);
    vga_write("  Type 'help' for available commands.\n\n");
    vga_write("  Initializing...\n");

    /* 1. 物理内存 — 从 Multiboot 信息获取 */
    /* 简单方式: 用 mem_upper 字段 (位于 mb_info+4, 单位 KB) */
    uint32 *mb = (uint32*)mb_info;
    uint32 mem_upper = 0;
    if (mb_info && (mb[0] & 1)) {  /* flags bit 0 = mem_* 有效 */
        mem_upper = *(uint32*)(mb_info + 4);
    }
    if (mem_upper == 0) mem_upper = 65536;  /* 回退: 64MB */

    mm_init(mem_upper);

    /* 2. 分页 */
    paging_init();

    /* 3. 中断 */
    interrupt_init();

    /* 4. ATA 磁盘 */
    ata_init();

    /* 5. FAT32 文件系统 */
    mounted_fs = fat32_mount();
    if (mounted_fs) {
        vga_write("  [OK] FAT32 filesystem mounted\n");
    } else {
        vga_write("  [!!] No FAT32 filesystem found (use 'ls' after attaching disk)\n");
    }

    /* 6. Shell */
    shell_init();
    vga_write("\n");

    shell_run();
}