/*
 * settings.c - 系统设置模块
 * 文本模式系统设置界面: 系统信息/显示/网络/隐私/关于
 * 使用 VGA 直接写入实现 UI 渲染
 */
#include "apps.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include "string.h"
#include "version.h"
#include "memory.h"

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

static inline void vga_text_n(int x, int y, const char *s, int n, uint8_t color)
{
    for (int i = 0; i < n && s[i]; i++)
        VGA_BUF[y * VGA_WIDTH + x + i] = (uint16_t)s[i] | (uint16_t)color << 8;
}

static uint8_t color(uint8_t fg, uint8_t bg) { return fg | bg << 4; }

/* 颜色定义 */
#define BG_CLR        color(VGA_COLOR_WHITE, VGA_COLOR_BLUE)
#define TITLE_BG      color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY)
#define SIDEBAR_BG    color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY)
#define SIDEBAR_SEL   color(VGA_COLOR_BLACK, VGA_COLOR_CYAN)
#define SIDEBAR_NORM  color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY)
#define CONTENT_BG    color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK)
#define GROUP_BG      color(VGA_COLOR_BLACK, VGA_COLOR_DARK_GREY)
#define LABEL_CLR     color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK)
#define VALUE_CLR     color(VGA_COLOR_WHITE, VGA_COLOR_BLACK)
#define HEADER_CLR    color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK)
#define ACCENT_CLR    color(VGA_COLOR_GREEN, VGA_COLOR_BLACK)
#define WARN_CLR      color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK)
#define MUTED_CLR     color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK)
#define STATUS_BG     color(VGA_COLOR_WHITE, VGA_COLOR_BLUE)
#define HELP_BG       color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY)
#define TOGGLE_ON     color(VGA_COLOR_GREEN, VGA_COLOR_BLACK)
#define TOGGLE_OFF    color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK)

/* Box-drawing */
#define HL 0xC4; #define VL 0xB3; #define TL 0xDA; #define TR 0xBF
#define BL 0xC0; #define BR 0xD9; #define BLOCK 0xDB

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

/* 侧边栏项目 */
static const char *sidebar_items[] = {
    "  系统信息    ",
    "  显示设置    ",
    "  网络设置    ",
    "  隐私安全    ",
    "  关于系统    ",
    NULL
};
static int sidebar_count = 5;

/* 设置状态 */
static int dark_mode = 1;
static int transparency = 1;
static int animations = 1;
static int firewall = 1;
static int auto_update = 1;
static int wallpaper_idx = 0;
static const char *wallpapers[] = { "默认", "暗色", "宇宙", NULL };

/* ---- 绘制侧边栏 ---- */
static void draw_sidebar(int sel)
{
    int sx = 0, sy = 1, sw = 16, sh = 23;
    vga_fill(sx, sy, sw, sh, ' ', SIDEBAR_BG);
    vga_text(1, 1, "  设置", color(VGA_COLOR_BLACK, VGA_COLOR_DARK_GREY));
    vga_text(1, 2, "  ────────", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));

    for (int i = 0; i < sidebar_count; i++) {
        int row = sy + 3 + i;
        if (i == sel) {
            vga_fill(sx + 1, row, sw - 2, 1, ' ', SIDEBAR_SEL);
            vga_text(sx + 2, row, sidebar_items[i], SIDEBAR_SEL);
        } else {
            vga_text(sx + 2, row, sidebar_items[i], SIDEBAR_NORM);
        }
    }
}

/* ---- 绘制系统信息页 ---- */
static void draw_sysinfo(void)
{
    int cx = 17, cy = 1;
    vga_fill(cx, cy, 63, 22, ' ', CONTENT_BG);
    vga_text(cx + 2, cy + 1, "系统信息", HEADER_CLR);

    /* 设备信息组 */
    int gy = cy + 3;
    vga_fill(cx + 2, gy, 58, 8, ' ', GROUP_BG);
    draw_box(cx + 2, gy, 58, 8, color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    vga_text(cx + 4, gy + 1, "设备信息", color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));

    vga_text(cx + 4, gy + 3, "设备名称:", LABEL_CLR);
    vga_text(cx + 16, gy + 3, "HU-Basic-PC", VALUE_CLR);

    vga_text(cx + 4, gy + 4, "处理器:", LABEL_CLR);
    vga_text(cx + 16, gy + 4, "x86 QEMU Virtual CPU", VALUE_CLR);

    vga_text(cx + 4, gy + 5, "内存:", LABEL_CLR);
    vga_text(cx + 16, gy + 5, "256 MB", VALUE_CLR);

    vga_text(cx + 4, gy + 6, "系统类型:", LABEL_CLR);
    vga_text(cx + 16, gy + 6, "32-bit / 64-bit 双内核", VALUE_CLR);

    /* 版本信息组 */
    gy = cy + 12;
    vga_fill(cx + 2, gy, 58, 8, ' ', GROUP_BG);
    draw_box(cx + 2, gy, 58, 8, color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    vga_text(cx + 4, gy + 1, "系统版本 (v2.0)", color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));

    vga_text(cx + 4, gy + 3, "系统版本:", LABEL_CLR);
    vga_text(cx + 16, gy + 3, BASIC_OS_FULL_STRING, ACCENT_CLR);

    vga_text(cx + 4, gy + 4, "内核版本:", LABEL_CLR);
    vga_text(cx + 16, gy + 4, KERNEL_VERSION_STR, VALUE_CLR);

    vga_text(cx + 4, gy + 5, "构建号:", LABEL_CLR);
    vga_text(cx + 16, gy + 5, BASIC_OS_BUILD_STR, VALUE_CLR);

    vga_text(cx + 4, gy + 6, "桌面环境:", LABEL_CLR);
    vga_text(cx + 16, gy + 6, DESKTOP_VERSION_STR, VALUE_CLR);
}

/* ---- 绘制显示设置页 ---- */
static void draw_display(void)
{
    int cx = 17, cy = 1;
    vga_fill(cx, cy, 63, 22, ' ', CONTENT_BG);
    vga_text(cx + 2, cy + 1, "显示设置", HEADER_CLR);

    int gy = cy + 3;
    vga_fill(cx + 2, gy, 58, 10, ' ', GROUP_BG);
    draw_box(cx + 2, gy, 58, 10, color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    vga_text(cx + 4, gy + 1, "桌面", color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));

    vga_text(cx + 4, gy + 3, "桌面背景:", LABEL_CLR);
    vga_text(cx + 16, gy + 3, wallpapers[wallpaper_idx], VALUE_CLR);

    vga_text(cx + 4, gy + 5, "暗色模式:", LABEL_CLR);
    vga_text(cx + 16, gy + 5, dark_mode ? "[ ON  ]" : "[ OFF ]",
             dark_mode ? TOGGLE_ON : TOGGLE_OFF);

    vga_text(cx + 4, gy + 6, "透明效果:", LABEL_CLR);
    vga_text(cx + 16, gy + 6, transparency ? "[ ON  ]" : "[ OFF ]",
             transparency ? TOGGLE_ON : TOGGLE_OFF);

    vga_text(cx + 4, gy + 7, "动画效果:", LABEL_CLR);
    vga_text(cx + 16, gy + 7, animations ? "[ ON  ]" : "[ OFF ]",
             animations ? TOGGLE_ON : TOGGLE_OFF);

    vga_text(cx + 4, gy + 9, "按 Enter 切换选项", MUTED_CLR);
}

/* ---- 绘制网络设置页 ---- */
static void draw_network(void)
{
    int cx = 17, cy = 1;
    vga_fill(cx, cy, 63, 22, ' ', CONTENT_BG);
    vga_text(cx + 2, cy + 1, "网络设置", HEADER_CLR);

    int gy = cy + 3;
    vga_fill(cx + 2, gy, 58, 10, ' ', GROUP_BG);
    draw_box(cx + 2, gy, 58, 10, color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    vga_text(cx + 4, gy + 1, "连接状态", color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));

    vga_text(cx + 4, gy + 3, "状态:", LABEL_CLR);
    vga_text(cx + 16, gy + 3, "已连接", ACCENT_CLR);

    vga_text(cx + 4, gy + 4, "IP 地址:", LABEL_CLR);
    vga_text(cx + 16, gy + 4, "10.0.2.15", VALUE_CLR);

    vga_text(cx + 4, gy + 5, "网关:", LABEL_CLR);
    vga_text(cx + 16, gy + 5, "10.0.2.2", VALUE_CLR);

    vga_text(cx + 4, gy + 6, "DNS:", LABEL_CLR);
    vga_text(cx + 16, gy + 6, "8.8.8.8", VALUE_CLR);

    vga_text(cx + 4, gy + 7, "MAC 地址:", LABEL_CLR);
    vga_text(cx + 16, gy + 7, "52:54:00:12:34:56", VALUE_CLR);
}

/* ---- 绘制隐私安全页 ---- */
static void draw_privacy(void)
{
    int cx = 17, cy = 1;
    vga_fill(cx, cy, 63, 22, ' ', CONTENT_BG);
    vga_text(cx + 2, cy + 1, "隐私与安全", HEADER_CLR);

    int gy = cy + 3;
    vga_fill(cx + 2, gy, 58, 10, ' ', GROUP_BG);
    draw_box(cx + 2, gy, 58, 10, color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    vga_text(cx + 4, gy + 1, "安全设置", color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));

    vga_text(cx + 4, gy + 3, "防火墙:", LABEL_CLR);
    vga_text(cx + 16, gy + 3, firewall ? "[ ON  ]" : "[ OFF ]",
             firewall ? TOGGLE_ON : TOGGLE_OFF);

    vga_text(cx + 4, gy + 5, "自动更新:", LABEL_CLR);
    vga_text(cx + 16, gy + 5, auto_update ? "[ ON  ]" : "[ OFF ]",
             auto_update ? TOGGLE_ON : TOGGLE_OFF);

    vga_text(cx + 4, gy + 7, "数据管理", color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
    vga_text(cx + 4, gy + 9, "按 C 清除缓存", color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
}

/* ---- 绘制关于页 ---- */
static void draw_about(void)
{
    int cx = 17, cy = 1;
    vga_fill(cx, cy, 63, 22, ' ', CONTENT_BG);

    vga_text(cx + 20, cy + 2, "HU basic OS", color(VGA_COLOR_CYAN, VGA_COLOR_BLACK));
    vga_text(cx + 18, cy + 4, "v" BASIC_OS_VERSION_STR, ACCENT_CLR);
    vga_text(cx + 12, cy + 6, "一个用于学习的 x86 操作系统内核", MUTED_CLR);

    int gy = cy + 8;
    vga_fill(cx + 2, gy, 58, 12, ' ', GROUP_BG);
    draw_box(cx + 2, gy, 58, 12, color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    vga_text(cx + 4, gy + 1, "技术规格", color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));

    vga_text(cx + 4, gy + 3, "架构:", LABEL_CLR);
    vga_text(cx + 16, gy + 3, "x86 / x86_64", VALUE_CLR);

    vga_text(cx + 4, gy + 4, "引导:", LABEL_CLR);
    vga_text(cx + 16, gy + 4, "Multiboot / Multiboot2", VALUE_CLR);

    vga_text(cx + 4, gy + 5, "显示:", LABEL_CLR);
    vga_text(cx + 16, gy + 5, "VGA 文本模式 80x25", VALUE_CLR);

    vga_text(cx + 4, gy + 6, "内核类型:", LABEL_CLR);
    vga_text(cx + 16, gy + 6, "单内核 (Monolithic)", VALUE_CLR);

    vga_text(cx + 4, gy + 7, "许可证:", LABEL_CLR);
    vga_text(cx + 16, gy + 7, "MIT License", VALUE_CLR);

    vga_text(cx + 4, gy + 8, "GitHub:", LABEL_CLR);
    vga_text(cx + 16, gy + 8, "HuHuBasic", VALUE_CLR);

    vga_text(cx + 4, gy + 10, BASIC_OS_COPYRIGHT, MUTED_CLR);
}

/* ---- 绘制整个设置界面 ---- */
static void draw_all(int sel, int tab)
{
    draw_sidebar(sel);
    switch (tab) {
        case 0: draw_sysinfo(); break;
        case 1: draw_display(); break;
        case 2: draw_network(); break;
        case 3: draw_privacy(); break;
        case 4: draw_about(); break;
    }
    /* 底部帮助栏 */
    vga_fill(0, 23, 80, 1, ' ', HELP_BG);
    vga_text(1, 23, "  Tab=切换面板  Enter=修改  W/S=切换壁纸  C=清除缓存  Q=退出",
             color(VGA_COLOR_BLACK, VGA_COLOR_DARK_GREY));
    vga_text(60, 23, "Settings v2.0", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));
}

/* 弹窗提示 */
static void show_popup(const char *msg)
{
    int mx = 20, my = 10, mw = 40, mh = 5;
    uint16_t saved[40 * 5];
    for (int row = 0; row < mh; row++)
        for (int col = 0; col < mw; col++)
            saved[row * mw + col] = VGA_BUF[(my + row) * VGA_WIDTH + (mx + col)];

    vga_fill(mx, my, mw, mh, ' ', color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    draw_box(mx, my, mw, mh, color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    vga_text(mx + 2, my + 2, msg, color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    vga_text(mx + 2, my + 3, "按任意键继续...", MUTED_CLR);

    while (keyboard_poll() == 0 && keyboard_getchar_nonblock() == 0)
        __asm__ volatile("hlt");

    for (int row = 0; row < mh; row++)
        for (int col = 0; col < mw; col++)
            VGA_BUF[(my + row) * VGA_WIDTH + (mx + col)] = saved[row * mw + col];
}

/* ---- 应用入口 ---- */
void app_settings(void)
{
    int sel = 0;
    int tab = 0;

    vga_fill(0, 0, 80, 25, ' ', BG_CLR);
    /* 标题栏 */
    vga_fill(0, 0, 80, 1, ' ', TITLE_BG);
    vga_text(1, 0, "  HU basic OS 系统设置", TITLE_BG);
    vga_text(30, 0, "v" BASIC_OS_VERSION_STR, color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));

    draw_all(sel, tab);

    while (1) {
        int key = keyboard_poll();
        if (key == KEY_NONE) {
            char c = keyboard_getchar_nonblock();
            if (c == 0) { __asm__ volatile("hlt"); continue; }

            if (c == 'q' || c == 'Q') {
                terminal_clear();
                return;
            }

            /* 切换选项 */
            if (c == '\n' || c == '\r') {
                if (tab == 1) {
                    if (sel == 0) { /* 暗色模式 */
                        dark_mode = !dark_mode;
                    } else if (sel == 1) { /* 透明效果 */
                        transparency = !transparency;
                    } else if (sel == 2) { /* 动画 */
                        animations = !animations;
                    }
                    draw_all(sel, tab);
                } else if (tab == 3) {
                    if (sel == 0) { firewall = !firewall; }
                    else if (sel == 1) { auto_update = !auto_update; }
                    draw_all(sel, tab);
                }
            }

            if (c == 'w' || c == 'W') {
                if (tab == 1 && sel == 0) {
                    wallpaper_idx = (wallpaper_idx + 1) % 3;
                    draw_all(sel, tab);
                }
            }
            if (c == 's' || c == 'S') {
                if (tab == 1 && sel == 0) {
                    wallpaper_idx = (wallpaper_idx + 2) % 3;
                    draw_all(sel, tab);
                }
            }
            if (c == 'c' || c == 'C') {
                if (tab == 3) {
                    show_popup("缓存已清除!");
                    draw_all(sel, tab);
                }
            }
            continue;
        }

        if (key == KEY_UP) {
            sel = (sel > 0) ? sel - 1 : sidebar_count - 1;
            tab = sel;
            draw_all(sel, tab);
        } else if (key == KEY_DOWN) {
            sel = (sel < sidebar_count - 1) ? sel + 1 : 0;
            tab = sel;
            draw_all(sel, tab);
        } else if (key == KEY_ENTER) {
            if (tab == 1) {
                if (sel == 0) dark_mode = !dark_mode;
                else if (sel == 1) transparency = !transparency;
                else if (sel == 2) animations = !animations;
            } else if (tab == 3) {
                if (sel == 0) firewall = !firewall;
                else if (sel == 1) auto_update = !auto_update;
            }
            draw_all(sel, tab);
        } else if (key == KEY_ESC || key == KEY_LEFT) {
            terminal_clear();
            return;
        } else if (key == KEY_TAB) {
            tab = sel;
            draw_all(sel, tab);
        }
    }
}