/*
 * kernel.c - 内核主入口
 * 初始化所有子系统，启动桌面环境
 */
#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "irq.h"
#include "isr.h"
#include "keyboard.h"
#include "timer.h"
#include "memory.h"
#include "shell.h"
#include "apps.h"
#include "desktop.h"
#include "ports.h"
#include "string.h"
#include "net/rtl8139.h"
#include "net/network.h"
#include "mouse.h"

/* 来自 boot.s 的 Multiboot 信息 */
extern uint32_t multiboot_magic;
extern uint32_t multiboot_info;

/* Multiboot 信息结构体 */
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
} multiboot_info_t;

/* 内核入口 */
void kernel_main(void)
{
    /* 1. 初始化 VGA 终端 */
    terminal_init();
    terminal_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_print("Basic Kernel v1.1 - Booting...\n\n");

    /* 2. 初始化 GDT */
    terminal_print("[OK] Initializing GDT...\n");
    gdt_init();

    /* 3. 初始化 IDT */
    terminal_print("[OK] Initializing IDT...\n");
    idt_init();

    /* 4. 初始化 IRQ */
    terminal_print("[OK] Initializing IRQ...\n");
    irq_init();

    /* 5. 启用中断 */
    terminal_print("[OK] Enabling interrupts...\n");
    __asm__ volatile("sti");

    /* 6. 初始化定时器 (100 Hz) */
    terminal_print("[OK] Initializing PIT (100 Hz)...\n");
    timer_init(100);

    /* 7. 初始化键盘 */
    terminal_print("[OK] Initializing Keyboard...\n");
    keyboard_init();

    /* 8. 初始化鼠标 */
    terminal_print("[OK] Initializing Mouse...\n");
    mouse_init();

    /* 8. 初始化内存管理 */
    terminal_print("[OK] Initializing Memory Manager...\n");
    memory_init();

    /* 9. 初始化应用管理器 */
    terminal_print("[OK] Initializing App Manager...\n");
    apps_init();

    /* 10. 初始化网络 */
    terminal_print("[OK] Initializing Network...\n");
    if (rtl8139_init() == 0) {
        terminal_print("[OK] RTL8139 NIC detected\n");
        network_init();
    } else {
        terminal_print("[WARN] No RTL8139 NIC found\n");
    }

    /* 11. 显示 Multiboot 信息 */
    terminal_print("[OK] Multiboot: magic=");
    terminal_print_hex(multiboot_magic);
    terminal_print(", info=");
    terminal_print_hex(multiboot_info);

    if (multiboot_magic == 0x2BADB002) {
        multiboot_info_t *mbi = (multiboot_info_t *)multiboot_info;
        terminal_print("\n[OK] Memory: lower=");
        terminal_print_dec(mbi->mem_lower);
        terminal_print("KB, upper=");
        terminal_print_dec(mbi->mem_upper);
        terminal_print("KB\n");
    } else {
        terminal_print("\n[WARN] Invalid Multiboot magic!\n");
    }

    terminal_print("\n[OK] Loading Desktop...\n");

    /* 11. 初始化桌面 */
    desktop_init();

    /* 主循环: 桌面 ←→ 终端 */
    while (1) {
        /* 进入桌面 */
        desktop_run();

        /* 用户选择了终端, 启动 Shell */
        terminal_clear();
        terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
        terminal_print("============================================\n");
        terminal_print("       Basic Kernel - Terminal Mode\n");
        terminal_print("============================================\n");
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_print("输入 'help' 查看命令, 输入 'desktop' 返回桌面\n\n");

        shell_run();

        /* Shell 退出 (reboot), 重启或回到桌面 */
        terminal_clear();
        terminal_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_print("Returning to Desktop...\n");
        desktop_init();
    }
}