/*
 * upgrade.c - 系统升级模块
 * 模拟系统升级流程: 检查更新、下载、安装、重启
 */
#include "apps.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include "string.h"
#include "version.h"
#include "ports.h"

#define VGA_BUF ((uint16_t *)0xB8000)

static inline void vga_put(int x, int y, char c, uint8_t color)
{
    VGA_BUF[y * VGA_WIDTH + x] = (uint16_t)c | (uint16_t)color << 8;
}

static inline void vga_fill(int x, int y, int w, int h, char c, uint8_t color)
{
    uint16_t entry = (uint16_t)c | (uint16_t)color << 8;
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            VGA_BUF[row * VGA_WIDTH + col] = entry;
}

static inline void vga_text(int x, int y, const char *s, uint8_t color)
{
    while (*s) { VGA_BUF[y * VGA_WIDTH + x] = (uint16_t)(*s) | (uint16_t)color << 8; x++; s++; }
}

static uint8_t color(uint8_t fg, uint8_t bg) { return fg | bg << 4; }

#define BG_CLR      color(VGA_COLOR_WHITE, VGA_COLOR_BLUE)
#define TITLE_BG    color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY)
#define CONTENT_BG  color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK)
#define HEADER_CLR  color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK)
#define ACCENT_CLR  color(VGA_COLOR_GREEN, VGA_COLOR_BLACK)
#define MUTED_CLR   color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK)
#define WARN_CLR    color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK)
#define HELP_BG     color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY)
#define LABEL_CLR   color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK)
#define VALUE_CLR   color(VGA_COLOR_WHITE, VGA_COLOR_BLACK)
#define GROUP_BG    color(VGA_COLOR_BLACK, VGA_COLOR_DARK_GREY)

static void draw_box(int x, int y, int w, int h, uint8_t clr)
{
    for (int row = y; row < y + h; row++) {
        for (int col = x; col < x + w; col++) {
            if (row == y && col == x)           vga_put(col, row, 0xDA, clr);
            else if (row == y && col == x+w-1)  vga_put(col, row, 0xBF, clr);
            else if (row == y+h-1 && col == x)  vga_put(col, row, 0xC0, clr);
            else if (row == y+h-1 && col == x+w-1) vga_put(col, row, 0xD9, clr);
            else if (row == y) vga_put(col, row, 0xC4, clr);
            else if (row == y+h-1) vga_put(col, row, 0xC4, clr);
            else if (col == x)  vga_put(col, row, 0xB3, clr);
            else if (col == x+w-1) vga_put(col, row, 0xB3, clr);
        }
    }
}

/* 绘制进度条 */
static void draw_progress(int x, int y, int w, int percent)
{
    int filled = (w * percent) / 100;
    for (int i = 0; i < w; i++) {
        char c = (i < filled) ? 0xDB : ' ';
        uint8_t clr = (i < filled) ? color(VGA_COLOR_GREEN, VGA_COLOR_BLACK)
                                   : color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY);
        vga_put(x + i, y, c, clr);
    }
}

/* 等待按键或超时 */
static int wait_key_or_timeout(int timeout_ms)
{
    uint32_t start = timer_get_ticks();
    while (1) {
        if (keyboard_getchar_nonblock() != 0) return 1;
        if (timer_get_ticks() - start >= (uint32_t)(timeout_ms / 10)) return 0;
        __asm__ volatile("hlt");
    }
}

/* 带进度条的模拟操作 */
static void simulate_progress(const char *msg, int x, int y, int w, int duration_ms)
{
    vga_text(x, y - 2, msg, MUTED_CLR);
    for (int p = 0; p <= 100; p += 5) {
        draw_progress(x, y, w, p);
        /* 百分比文本 */
        char pct[8];
        pct[0] = '0' + (p / 100) % 10;
        pct[1] = '0' + ((p / 10) % 10);
        pct[2] = '0' + (p % 10);
        pct[3] = '%';
        pct[4] = '\0';
        vga_text(x + w + 2, y, pct, ACCENT_CLR);

        uint32_t start = timer_get_ticks();
        int step_ms = duration_ms / 20;
        while (timer_get_ticks() - start < (uint32_t)(step_ms / 10)) {
            __asm__ volatile("hlt");
        }
    }
}

/* 更新日志列表 */
static const char *changelog[] = {
    "v2.0.0  全新桌面环境、系统设置、系统升级、应用商店、现代浏览器",
    "v1.1.0  新增 64+32 双内核支持、QEMU 配置文件",
    "v1.0.0  初始版本: GDT/IDT、VGA 显示、键盘驱动、内存管理、Shell",
    NULL
};

void app_upgrade(void)
{
    int stage = 0; /* 0=主界面, 1=检查中, 2=发现更新, 3=下载中, 4=完成 */
    int update_available = 0;

    vga_fill(0, 0, 80, 25, ' ', BG_CLR);
    vga_fill(0, 0, 80, 1, ' ', TITLE_BG);
    vga_text(1, 0, "  HU basic OS 系统升级", TITLE_BG);
    vga_text(30, 0, "v" BASIC_OS_VERSION_STR, color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));

    while (1) {
        /* 绘制主界面 */
        if (stage == 0) {
            vga_fill(0, 1, 80, 22, ' ', CONTENT_BG);

            /* 当前版本 */
            int gy = 3;
            vga_fill(5, gy, 70, 3, ' ', GROUP_BG);
            draw_box(5, gy, 70, 3, color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
            vga_text(7, gy + 1, "当前版本: ", LABEL_CLR);
            vga_text(20, gy + 1, BASIC_OS_FULL_STRING, ACCENT_CLR);
            vga_text(50, gy + 1, "[ 检查更新 ]", color(VGA_COLOR_BLACK, VGA_COLOR_CYAN));

            /* 更新日志 */
            gy = 8;
            vga_text(7, gy, "更新日志", HEADER_CLR);
            for (int i = 0; changelog[i]; i++) {
                vga_text(9, gy + 2 + i * 2, changelog[i], VALUE_CLR);
            }

            /* 底部帮助 */
            vga_fill(0, 23, 80, 1, ' ', HELP_BG);
            vga_text(1, 23, "  Enter=检查更新  Q=退出",
                     color(VGA_COLOR_BLACK, VGA_COLOR_DARK_GREY));
        }

        /* 等待输入 */
        int key = keyboard_poll();
        if (key == KEY_NONE) {
            char c = keyboard_getchar_nonblock();
            if (c == 0) { __asm__ volatile("hlt"); continue; }

            if (c == 'q' || c == 'Q') {
                if (stage == 0) {
                    terminal_clear();
                    return;
                }
            }

            if ((c == '\n' || c == '\r') && stage == 0) {
                stage = 1;
                /* 清除内容区 */
                vga_fill(0, 1, 80, 22, ' ', CONTENT_BG);
                vga_text(10, 6, "正在连接更新服务器...", MUTED_CLR);
                draw_progress(10, 8, 55, 0);

                /* 模拟检查进度 */
                simulate_progress("正在检查更新...", 10, 8, 55, 2000);

                /* 显示结果 */
                vga_fill(0, 1, 80, 22, ' ', CONTENT_BG);
                vga_text(10, 5, "发现新版本!", ACCENT_CLR);
                vga_text(10, 7, "Basic OS v2.0.1 — 包含性能优化和 Bug 修复", VALUE_CLR);
                vga_text(10, 9, "大小: 256 KB", MUTED_CLR);
                vga_text(10, 10, "发布日期: 2026-07-26", MUTED_CLR);

                vga_text(10, 13, "[ Enter ] 立即安装更新", color(VGA_COLOR_BLACK, VGA_COLOR_GREEN));
                vga_text(10, 14, "[ Q ] 稍后提醒", MUTED_CLR);
                update_available = 1;
                stage = 2;
            }

            if ((c == '\n' || c == '\r') && stage == 2 && update_available) {
                stage = 3;
                vga_fill(0, 1, 80, 22, ' ', CONTENT_BG);
                vga_text(10, 4, "正在安装更新...", HEADER_CLR);

                /* 模拟下载 */
                simulate_progress("正在下载 v2.0.1...", 10, 6, 55, 2500);
                /* 模拟安装 */
                simulate_progress("正在安装更新...", 10, 9, 55, 2000);
                /* 模拟配置 */
                simulate_progress("正在配置系统...", 10, 12, 55, 1500);

                vga_fill(0, 1, 80, 22, ' ', CONTENT_BG);
                vga_text(10, 6, "更新安装完成!", ACCENT_CLR);
                vga_text(10, 8, "系统已更新至 Basic OS v2.0.1", VALUE_CLR);
                vga_text(10, 10, "需要重启系统以应用更新。", MUTED_CLR);

                vga_text(10, 14, "[ Enter ] 立即重启", color(VGA_COLOR_BLACK, VGA_COLOR_GREEN));
                vga_text(10, 15, "[ Q ] 稍后重启", MUTED_CLR);
                stage = 4;
            }

            if ((c == '\n' || c == '\r') && stage == 4) {
                /* 模拟重启 - 通过 keyboard controller 重启 */
                terminal_clear();
                terminal_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
                terminal_print("系统正在重启以应用更新...\n\n");
                for (volatile int i = 0; i < 5000000; i++) { __asm__ volatile("nop"); }
                /* CPU 三重故障重启 */
                uint8_t status;
                do { status = inb(0x64); } while (status & 0x02);
                outb(0x64, 0xFE);
                __asm__ volatile("int $0");
            }

            continue;
        }

        if (key == KEY_ENTER && stage == 0) {
            stage = 1;
            /* 同上 */
            vga_fill(0, 1, 80, 22, ' ', CONTENT_BG);
            vga_text(10, 6, "正在连接更新服务器...", MUTED_CLR);
            draw_progress(10, 8, 55, 0);
            simulate_progress("正在检查更新...", 10, 8, 55, 2000);
            vga_fill(0, 1, 80, 22, ' ', CONTENT_BG);
            vga_text(10, 5, "发现新版本!", ACCENT_CLR);
            vga_text(10, 7, "Basic OS v2.0.1 — 包含性能优化和 Bug 修复", VALUE_CLR);
            vga_text(10, 9, "大小: 256 KB", MUTED_CLR);
            vga_text(10, 10, "发布日期: 2026-07-26", MUTED_CLR);
            vga_text(10, 13, "[ Enter ] 立即安装更新", color(VGA_COLOR_BLACK, VGA_COLOR_GREEN));
            vga_text(10, 14, "[ Q ] 稍后提醒", MUTED_CLR);
            update_available = 1;
            stage = 2;
        }

        if (key == KEY_ESC) {
            if (stage == 0) {
                terminal_clear();
                return;
            }
        }
    }
}