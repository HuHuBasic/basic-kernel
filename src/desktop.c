/*
 * desktop.c - 文本模式桌面环境
 * 模拟桌面: 图标区 + 底部面板 + 开始菜单 + 窗口
 */
#include "desktop.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include "memory.h"
#include "string.h"
#include "apps.h"
#include "ports.h"
#include "mouse.h"

/* 桌面常量 */
#define MAX_DESKTOP_ICONS 12

/* ---- VGA 直接写入 ---- */
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
    while (*s) {
        VGA_BUF[y * VGA_WIDTH + x] = (uint16_t)(*s) | (uint16_t)color << 8;
        x++; s++;
    }
}

static uint8_t color(uint8_t fg, uint8_t bg) { return fg | bg << 4; }

/* ---- 桌面状态 ---- */
#define DESKTOP_BG      color(VGA_COLOR_WHITE, VGA_COLOR_BLUE)
#define PANEL_COLOR     color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY)
#define ICON_COLOR      color(VGA_COLOR_WHITE, VGA_COLOR_BLUE)
#define ICON_SEL_COLOR  color(VGA_COLOR_YELLOW, VGA_COLOR_BLUE)
#define ICON_LABEL_CLR  color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLUE)
#define MENU_BG         color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY)
#define MENU_SEL        color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY)
#define WIN_TITLE       color(VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE)
#define WIN_BODY        color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK)
#define WIN_BORDER      color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK)

/* 桌面图标定义 */
typedef struct {
    int  x, y;          /* 位置 */
    int  w, h;          /* 大小 */
    const char *icon[4]; /* 图标 ASCII */
    const char *label;   /* 名称 */
    const char *app;     /* 关联应用名 */
    int  installed;      /* 是否已安装 */
} desktop_icon_t;

static int sel_icon = 0;
static int icon_count = 0;
static int menu_open = 0;
static int menu_sel = 0;
static int desktop_running = 1;

static desktop_icon_t icons[MAX_DESKTOP_ICONS];

/* 开始菜单项 */
static const char *menu_items[] = {
    "  Terminal    ",
    "  App Center  ",
    "  Calculator  ",
    "  Guess Game  ",
    "  Monitor     ",
    "  ASCII Art   ",
    "  Browser     ",
    "  About       ",
    "  Shutdown    ",
    NULL
};
static int menu_count = 9;

/* ---- 桌面渲染 ---- */

/* Box-drawing characters */
#define HL 0xC4  /* ─ */
#define VL 0xB3  /* │ */
#define TL 0xDA  /* ┌ */
#define TR 0xBF  /* ┐ */
#define BL 0xC0  /* └ */
#define BR 0xD9  /* ┘ */
#define LT 0xC3  /* ├ */
#define RT 0xB4  /* ┤ */
#define TT 0xC2  /* ┬ */
#define BT 0xC1  /* ┴ */
#define CR 0xC5  /* ┼ */
#define BLOCK 0xDB

static void draw_box(int x, int y, int w, int h, uint8_t clr)
{
    for (int row = y; row < y + h; row++) {
        for (int col = x; col < x + w; col++) {
            if (row == y && col == x)           vga_put(col, row, TL, clr);
            else if (row == y && col == x+w-1)  vga_put(col, row, TR, clr);
            else if (row == y+h-1 && col == x)  vga_put(col, row, BL, clr);
            else if (row == y+h-1 && col == x+w-1) vga_put(col, row, BR, clr);
            else if (row == y) vga_put(col, row, HL, clr);
            else if (row == y+h-1) vga_put(col, row, HL, clr);
            else if (col == x)  vga_put(col, row, VL, clr);
            else if (col == x+w-1) vga_put(col, row, VL, clr);
        }
    }
}

static void draw_panel(void)
{
    int py = 23;
    /* 面板背景 */
    vga_fill(0, py, 80, 2, ' ', PANEL_COLOR);

    /* 开始按钮 */
    vga_put(0, py, ' ', color(VGA_COLOR_WHITE, VGA_COLOR_GREEN));
    vga_put(1, py, ' ', color(VGA_COLOR_WHITE, VGA_COLOR_GREEN));
    vga_text(2, py, "START", color(VGA_COLOR_BLACK, VGA_COLOR_GREEN));

    /* 分隔线 */
    vga_put(7, py, VL, color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));

    /* 已安装应用快捷方式 */
    int x = 9;
    for (int i = 0; i < icon_count; i++) {
        if (icons[i].installed) {
            vga_text(x, py, "| ", PANEL_COLOR);
            x += 2;
            vga_text(x, py, icons[i].label, PANEL_COLOR);
            x += strlen(icons[i].label);
            if (x > 60) break;
        }
    }

    /* 时钟 */
    uint32_t sec = timer_get_seconds();
    char time_buf[16];
    int h = sec / 3600;
    int m = (sec % 3600) / 60;
    int s = sec % 60;
    time_buf[0] = '0' + (h / 10) % 10;
    time_buf[1] = '0' + (h % 10);
    time_buf[2] = ':';
    time_buf[3] = '0' + (m / 10) % 10;
    time_buf[4] = '0' + (m % 10);
    time_buf[5] = ':';
    time_buf[6] = '0' + (s / 10) % 10;
    time_buf[7] = '0' + (s % 10);
    time_buf[8] = '\0';
    vga_text(70, py, time_buf, PANEL_COLOR);

    /* 版本 */
    vga_text(70, py + 1, "v1.1", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));
}

static void draw_icons(void)
{
    for (int i = 0; i < icon_count; i++) {
        int selected = (i == sel_icon && !menu_open);
        uint8_t ic = selected ? ICON_SEL_COLOR : ICON_COLOR;
        uint8_t lc = selected ? color(VGA_COLOR_YELLOW, VGA_COLOR_BLUE) : ICON_LABEL_CLR;

        /* 图标背景 (选中时高亮) */
        if (selected) {
            vga_fill(icons[i].x - 1, icons[i].y - 1,
                     icons[i].w + 2, icons[i].h + 2, ' ', color(VGA_COLOR_BLACK, VGA_COLOR_CYAN));
        }

        /* 绘制图标 */
        for (int row = 0; row < 4; row++) {
            vga_text(icons[i].x, icons[i].y + row, icons[i].icon[row], ic);
        }

        /* 标签 */
        int lx = icons[i].x + (icons[i].w - strlen(icons[i].label)) / 2;
        if (lx < 0) lx = 0;
        vga_text(lx, icons[i].y + 4, icons[i].label, lc);

        /* 状态标记 */
        if (icons[i].installed) {
            vga_put(icons[i].x + icons[i].w - 1, icons[i].y, '*', color(VGA_COLOR_GREEN, VGA_COLOR_BLUE));
        }
    }
}

static void draw_menu(void)
{
    if (!menu_open) return;

    int mx = 1, my = 16;
    int mw = 18, mh = menu_count + 2;

    /* 菜单背景 */
    vga_fill(mx, my, mw, mh, ' ', MENU_BG);
    draw_box(mx, my, mw, mh, MENU_BG);

    /* 菜单标题 */
    vga_text(mx + 2, my + 1, "  Start Menu  ", color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY));

    for (int i = 0; i < menu_count; i++) {
        uint8_t clr = (i == menu_sel) ? MENU_SEL : MENU_BG;
        vga_text(mx + 1, my + 2 + i, menu_items[i], clr);
    }
}

static void draw_status_bar(void)
{
    /* 顶部状态栏 */
    vga_fill(0, 0, 80, 1, ' ', color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY));
    vga_text(1, 0, "Basic Kernel Desktop", color(VGA_COLOR_BLACK, VGA_COLOR_DARK_GREY));
    vga_text(60, 0, "F1=Help  Esc=Menu", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));
}

static void draw_bg(void)
{
    vga_fill(0, 0, 80, 25, ' ', DESKTOP_BG);
}

/* ---- 弹窗 ---- */
static void show_message_box(const char *title, const char *msg)
{
    int mx = 15, my = 8, mw = 50, mh = 7;

    /* 保存桌面区域 */
    uint16_t saved[50 * 7];
    for (int row = 0; row < mh; row++)
        for (int col = 0; col < mw; col++)
            saved[row * mw + col] = VGA_BUF[(my + row) * VGA_WIDTH + (mx + col)];

    /* 绘制弹窗 */
    vga_fill(mx, my, mw, mh, ' ', WIN_BODY);
    draw_box(mx, my, mw, mh, WIN_BORDER);
    vga_text(mx + 2, my + 1, title, WIN_TITLE);
    vga_text(mx + 2, my + 3, msg, WIN_BODY);
    vga_text(mx + 2, my + 5, "Press any key to close...", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));

    /* 等待按键 */
    while (keyboard_poll() == 0 && keyboard_getchar_nonblock() == 0) {
        __asm__ volatile("hlt");
    }

    /* 恢复 */
    for (int row = 0; row < mh; row++)
        for (int col = 0; col < mw; col++)
            VGA_BUF[(my + row) * VGA_WIDTH + (mx + col)] = saved[row * mw + col];
}

/* ---- 桌面主逻辑 ---- */

void desktop_init(void)
{
    icon_count = 0;
    sel_icon = 0;
    menu_open = 0;
    menu_sel = 0;
    desktop_running = 1;

    /* 定义桌面图标 */
    int ix = 3, iy = 3;

    /* 终端 */
    icons[0].x = ix; icons[0].y = iy; icons[0].w = 8; icons[0].h = 4;
    icons[0].icon[0] = "  ____  "; icons[0].icon[1] = " |  _ \\ ";
    icons[0].icon[2] = " | |_) |"; icons[0].icon[3] = " |____/ ";
    icons[0].label = "Terminal"; icons[0].app = "terminal";
    icons[0].installed = 1;

    /* 计算器 */
    ix += 12;
    icons[1].x = ix; icons[1].y = iy; icons[1].w = 8; icons[1].h = 4;
    icons[1].icon[0] = "  __  __ "; icons[1].icon[1] = " |  \\/  |";
    icons[1].icon[2] = " | |\\/| |"; icons[1].icon[3] = " |_|  |_|";
    icons[1].label = "Calculator"; icons[1].app = "calculator";
    icons[1].installed = 0;

    /* 猜数字 */
    ix += 12;
    icons[2].x = ix; icons[2].y = iy; icons[2].w = 8; icons[2].h = 4;
    icons[2].icon[0] = "  ????  "; icons[2].icon[1] = " ?    ? ";
    icons[2].icon[2] = " ? ?? ? "; icons[2].icon[3] = "  ????  ";
    icons[2].label = "Guess"; icons[2].app = "guess";
    icons[2].installed = 0;

    /* 监控 */
    ix += 12;
    icons[3].x = ix; icons[3].y = iy; icons[3].w = 8; icons[3].h = 4;
    icons[3].icon[0] = "  /\\/\\  "; icons[3].icon[1] = " /    \\ ";
    icons[3].icon[2] = "|  ||  |"; icons[3].icon[3] = " \\____/ ";
    icons[3].label = "Monitor"; icons[3].app = "monitor";
    icons[3].installed = 0;

    /* ASCII */
    ix += 12;
    icons[4].x = ix; icons[4].y = iy; icons[4].w = 8; icons[4].h = 4;
    icons[4].icon[0] = "  _    _ "; icons[4].icon[1] = " | |  | |";
    icons[4].icon[2] = " | |__| |"; icons[4].icon[3] = "  \\____/ ";
    icons[4].label = "ASCII Art"; icons[4].app = "ascii";
    icons[4].installed = 0;

    /* 第二行 */
    ix = 3; iy = 10;
    icons[5].x = ix; icons[5].y = iy; icons[5].w = 8; icons[5].h = 4;
    icons[5].icon[0] = "  ____  "; icons[5].icon[1] = " |  _ \\ ";
    icons[5].icon[2] = " | | | |"; icons[5].icon[3] = " |_| |_|";
    icons[5].label = "About"; icons[5].app = "about";
    icons[5].installed = 1;

    ix += 12;
    icons[6].x = ix; icons[6].y = iy; icons[6].w = 8; icons[6].h = 4;
    icons[6].icon[0] = "  ____  "; icons[6].icon[1] = " |  _ \\ ";
    icons[6].icon[2] = " | |_) |"; icons[6].icon[3] = " |____/ ";
    icons[6].label = "Reboot"; icons[6].app = "reboot";
    icons[6].installed = 1;

    ix += 12;
    icons[7].x = ix; icons[7].y = iy; icons[7].w = 8; icons[7].h = 4;
    icons[7].icon[0] = "  ____  "; icons[7].icon[1] = " | __ ) ";
    icons[7].icon[2] = " |  _ \\ "; icons[7].icon[3] = " |____/ ";
    icons[7].label = "Browser"; icons[7].app = "browser";
    icons[7].installed = 1;

    ix += 12;
    icons[8].x = ix; icons[8].y = iy; icons[8].w = 8; icons[8].h = 4;
    icons[8].icon[0] = "  ____  "; icons[8].icon[1] = " |  _ \\ ";
    icons[8].icon[2] = " | | | |"; icons[8].icon[3] = " |____/ ";
    icons[8].label = "AppCenter"; icons[8].app = "appcenter";
    icons[8].installed = 1;

    icon_count = 9;

    /* 清除特殊键缓冲区 */
    keyboard_clear_special();
}

void desktop_run(void)
{
    draw_bg();
    draw_status_bar();
    draw_icons();
    draw_panel();
    terminal_update_cursor();

    while (desktop_running) {
        /* 刷新时钟 */
        static uint32_t last_time = 0;
        uint32_t now = timer_get_ticks();
        if (now - last_time >= 50) {
            last_time = now;
            mouse_hide();
            draw_panel();
            mouse_draw();
        }

        /* ---- 鼠标处理 ---- */
        int clicked = mouse_left_click();
        int moved = mouse_moved();
        if (moved || clicked) {
            mouse_hide();
            int mx = mouse_get_x();
            int my = mouse_get_y();

            if (clicked) {
                mouse_tap_feedback();
                /* 检查是否点击了图标 */
                for (int i = 0; i < icon_count; i++) {
                    if (mx >= icons[i].x && mx < icons[i].x + icons[i].w &&
                        my >= icons[i].y && my < icons[i].y + icons[i].h) {
                        sel_icon = i;
                        /* 启动图标 */
                        if (icons[i].installed) {
                            if (strcmp(icons[i].app, "terminal") == 0) {
                                desktop_running = 0;
                            } else if (strcmp(icons[i].app, "about") == 0) {
                                show_message_box("Basic Kernel v1.1",
                                    "x86 32-bit teaching OS kernel.\n"
                                    "Features: GDT/IDT/VGA/Keyboard/PIT/Memory/Apps/Desktop\n"
                                    "Built on " __DATE__);
                            } else if (strcmp(icons[i].app, "reboot") == 0) {
                                show_message_box("Reboot", "Use 'reboot' in terminal to restart.");
                            } else {
                                apps_run(icons[i].app);
                            }
                        } else {
                            show_message_box(icons[i].label, "App not installed.\nUse terminal to install: install <name>");
                        }
                        draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
                        if (menu_open) draw_menu();
                        break;
                    }
                }
                /* 检查是否点击了面板 (开始菜单) */
                if (my >= 24) {
                    menu_open = !menu_open;
                    menu_sel = 0;
                    if (menu_open) {
                        draw_menu();
                    } else {
                        draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
                    }
                }
            }

            /* 鼠标移动: 高亮对应图标 */
            if (!menu_open) {
                for (int i = 0; i < icon_count; i++) {
                    if (mx >= icons[i].x && mx < icons[i].x + icons[i].w &&
                        my >= icons[i].y && my < icons[i].y + icons[i].h) {
                        if (sel_icon != i) {
                            sel_icon = i;
                            draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
                        }
                        break;
                    }
                }
            } else {
                /* 菜单打开时: 鼠标悬停菜单项 */
                int menu_x = 1, menu_y = 19;
                if (mx >= menu_x && mx < menu_x + 16 && my >= menu_y && my < menu_y + menu_count) {
                    int item = my - menu_y;
                    if (item >= 0 && item < menu_count && item != menu_sel) {
                        menu_sel = item;
                        draw_menu();
                    }
                }
                /* 点击菜单项 */
                if (clicked) {
                    if (mx >= menu_x && mx < menu_x + 16 && my >= menu_y && my < menu_y + menu_count) {
                        int item = my - menu_y;
                        if (item >= 0 && item < menu_count) {
                            menu_sel = item;
                            /* 执行菜单项 */
                            switch (menu_sel) {
                                case 0: desktop_running = 0; break;
                                case 1: if (icons[8].installed) apps_run("appcenter"); break;
                                case 2: if (icons[1].installed) apps_run("calculator"); break;
                                case 3: if (icons[2].installed) apps_run("guess"); break;
                                case 4: if (icons[3].installed) apps_run("monitor"); break;
                                case 5: if (icons[4].installed) apps_run("ascii"); break;
                                case 6: if (icons[7].installed) apps_run("browser"); break;
                                case 7: show_message_box("Basic Kernel v1.1",
                                            "x86 32-bit teaching OS kernel.\n"
                                            "Features: GDT/IDT/VGA/Keyboard/PIT/Memory/Apps/Desktop\n"
                                            "Built on " __DATE__); break;
                                case 8: show_message_box("Reboot", "Use 'reboot' in terminal to restart."); break;
                            }
                            menu_open = 0;
                            draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
                        }
                    }
                }
            }
            mouse_draw();
        }

        int key = keyboard_poll();

        if (key == KEY_NONE) {
            /* 也检查普通字符 */
            char c = keyboard_getchar_nonblock();
            if (c == 0) {
                __asm__ volatile("hlt");
                continue;
            }
            /* 处理字母键 */
            if (c == 'm' || c == 'M') {
                menu_open = !menu_open;
                menu_sel = 0;
                if (menu_open) {
                    draw_menu();
                } else {
                    draw_bg();
                    draw_status_bar();
                    draw_icons();
                    draw_panel();
                }
            } else if (c == 'q' || c == 'Q') {
                desktop_running = 0;
            } else if (c == ' ') {
                /* 空格 = 启动选中图标 */
                if (!menu_open) {
                    key = KEY_ENTER; /* 复用逻辑 */
                }
            } else if (c >= '1' && c <= '9') {
                int idx = c - '1';
                if (idx < icon_count && !menu_open) {
                    sel_icon = idx;
                    draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
                    key = KEY_ENTER;
                }
            }
            else continue;
        }

        if (key == KEY_UP) {
            if (menu_open) {
                if (menu_sel > 0) {
                    menu_sel--;
                } else {
                    menu_sel = menu_count - 1;
                }
                draw_menu();
            } else {
                /* 向上选择图标 (循环) */
                int old_sel = sel_icon;
                if (sel_icon >= 5) {
                    sel_icon = sel_icon - 5;
                } else if (sel_icon > 0) {
                    sel_icon = sel_icon - 1;
                } else {
                    /* 第一行第一个: 跳到同列最后一行 */
                    sel_icon = icon_count - 1;
                }
                if (sel_icon != old_sel) {
                    draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
                    if (menu_open) draw_menu();
                }
            }
        } else if (key == KEY_DOWN) {
            if (menu_open) {
                if (menu_sel < menu_count - 1) {
                    menu_sel++;
                } else {
                    menu_sel = 0;
                }
                draw_menu();
            } else {
                int old_sel = sel_icon;
                if (sel_icon < 5 && sel_icon + 5 < icon_count) {
                    sel_icon = sel_icon + 5;
                } else if (sel_icon + 1 < icon_count) {
                    sel_icon = sel_icon + 1;
                } else {
                    /* 最后一个: 跳到第一个 */
                    sel_icon = 0;
                }
                if (sel_icon != old_sel) {
                    draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
                    if (menu_open) draw_menu();
                }
            }
        } else if (key == KEY_LEFT) {
            if (menu_open) {
                menu_open = 0;
                draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
            } else {
                if (sel_icon > 0) {
                    sel_icon--;
                } else {
                    sel_icon = icon_count - 1;
                }
                draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
            }
        } else if (key == KEY_RIGHT) {
            if (sel_icon < icon_count - 1) {
                sel_icon++;
            } else {
                sel_icon = 0;
            }
            draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
        } else if (key == KEY_ENTER) {
            if (menu_open) {
                /* 执行菜单项 */
                menu_open = 0;
                draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
                switch (menu_sel) {
                    case 0: /* Terminal */
                        desktop_running = 0;
                        break;
                    case 1: /* App Center */
                        if (icons[8].installed) apps_run("appcenter");
                        else show_message_box("App Center", "Not installed. Use 'install appcenter' in terminal.");
                        break;
                    case 2: /* Calculator */
                        if (icons[1].installed) apps_run("calculator");
                        else show_message_box("Calculator", "Not installed. Use 'install calculator' in terminal.");
                        break;
                    case 3: /* Guess */
                        if (icons[2].installed) apps_run("guess");
                        else show_message_box("Guess Game", "Not installed. Use 'install guess' in terminal.");
                        break;
                    case 4: /* Monitor */
                        if (icons[3].installed) apps_run("monitor");
                        else show_message_box("Monitor", "Not installed. Use 'install monitor' in terminal.");
                        break;
                    case 5: /* ASCII */
                        if (icons[4].installed) apps_run("ascii");
                        else show_message_box("ASCII Art", "Not installed. Use 'install ascii' in terminal.");
                        break;
                    case 6: /* Browser */
                        if (icons[7].installed) apps_run("browser");
                        else show_message_box("Browser", "Not installed. Use 'install browser' in terminal.");
                        break;
                    case 7: /* About */
                        show_message_box("Basic Kernel v1.1",
                            "x86 32-bit teaching OS kernel.\n"
                            "Features: GDT/IDT/VGA/Keyboard/PIT/Memory/Apps/Desktop");
                        break;
                    case 8: /* Shutdown */
                        desktop_running = 0;
                        break;
                }
                draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
            } else {
                /* 双击图标 */
                if (sel_icon >= 0 && sel_icon < icon_count) {
                    desktop_icon_t *ic = &icons[sel_icon];
                    if (ic->installed) {
                        if (strcmp(ic->app, "terminal") == 0) {
                            desktop_running = 0;
                        } else if (strcmp(ic->app, "about") == 0) {
                            show_message_box("Basic Kernel v1.1",
                                "x86 32-bit teaching OS kernel.\n"
                                "Features: GDT/IDT/VGA/Keyboard/PIT/Memory/Apps/Desktop\n"
                                "Built on " __DATE__);
                        } else if (strcmp(ic->app, "reboot") == 0) {
                            show_message_box("Reboot", "Use 'reboot' in terminal to restart.");
                        } else {
                            apps_run(ic->app);
                        }
                    } else {
                        show_message_box(ic->label, "App not installed.\nUse terminal to install: install <name>");
                    }
                    draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
                }
            }
        } else if (key == KEY_ESC) {
            if (menu_open) {
                menu_open = 0;
                draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
            } else {
                menu_open = 1;
                menu_sel = 0;
                draw_menu();
            }
        } else if (key == KEY_TAB) {
            sel_icon = (sel_icon + 1) % icon_count;
            draw_bg(); draw_status_bar(); draw_icons(); draw_panel();
        } else if (key == KEY_F1) {
            show_message_box("Desktop Help",
                "Arrow keys  - Move selection (cycle)\n"
                "Enter/Space  - Launch icon\n"
                "Esc          - Start Menu\n"
                "Tab          - Next icon\n"
                "1-9          - Select icon by number\n"
                "q            - Exit to terminal");
        }
    }

    /* 清屏退出 */
    terminal_clear();
}