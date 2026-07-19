/*
 * app_center.c - 应用中心
 * 图形化应用管理界面: 浏览、安装、运行、卸载
 */
#include "apps.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include "string.h"
#include "memory.h"
#include "mouse.h"

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

static inline void vga_text_n(int x, int y, const char *s, int n, uint8_t color)
{
    for (int i = 0; i < n && s[i]; i++) {
        VGA_BUF[y * VGA_WIDTH + x + i] = (uint16_t)s[i] | (uint16_t)color << 8;
    }
}

static uint8_t color(uint8_t fg, uint8_t bg) { return fg | bg << 4; }

/* ---- 颜色主题 ---- */
#define BG_CLR        color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLUE)
#define TITLE_BG      color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY)
#define TITLE_CLR     color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY)
#define LIST_BG       color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK)
#define LIST_SEL_BG   color(VGA_COLOR_BLACK, VGA_COLOR_CYAN)
#define LIST_INSTALLED color(VGA_COLOR_GREEN, VGA_COLOR_BLACK)
#define LIST_NOT_INST  color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK)
#define LIST_RUNNING   color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK)
#define DETAIL_BG     color(VGA_COLOR_WHITE, VGA_COLOR_BLACK)
#define DETAIL_LABEL  color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK)
#define DETAIL_VAL    color(VGA_COLOR_WHITE, VGA_COLOR_BLACK)
#define HELP_BG       color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY)
#define STATUS_BG     color(VGA_COLOR_WHITE, VGA_COLOR_BLUE)
#define MSG_OK        color(VGA_COLOR_GREEN, VGA_COLOR_BLACK)
#define MSG_ERR       color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK)
#define MSG_WARN      color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK)

/* Box-drawing */
#define HL 0xC4
#define VL 0xB3
#define TL 0xDA
#define TR 0xBF
#define BL 0xC0
#define BR 0xD9
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

/* 状态文本 */
static const char *state_text(int state)
{
    if (state == APP_STATE_RUNNING) return "运行中";
    if (state == APP_STATE_INSTALLED) return "已安装";
    return "未安装";
}

static uint8_t state_color(int state)
{
    if (state == APP_STATE_RUNNING) return LIST_RUNNING;
    if (state == APP_STATE_INSTALLED) return LIST_INSTALLED;
    return LIST_NOT_INST;
}

/* ---- 绘制状态栏 ---- */
static void draw_status_line(const char *msg, uint8_t msg_clr)
{
    vga_fill(0, 24, 80, 1, ' ', STATUS_BG);
    if (msg) vga_text(2, 24, msg, msg_clr);
    vga_text(65, 24, "F1=帮助", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLUE));
}

/* ---- 帮助弹窗 ---- */
static void show_help(void)
{
    int mx = 12, my = 6, mw = 56, mh = 12;
    uint16_t saved[56 * 12];
    for (int row = 0; row < mh; row++)
        for (int col = 0; col < mw; col++)
            saved[row * mw + col] = VGA_BUF[(my + row) * VGA_WIDTH + (mx + col)];

    vga_fill(mx, my, mw, mh, ' ', color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    draw_box(mx, my, mw, mh, color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    vga_text(mx + 2, my + 1, "应用中心 - 帮助", color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
    vga_text(mx + 2, my + 3, "  ↑ ↓   - 上下选择应用 (循环)", color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_text(mx + 2, my + 4, "  Enter - 安装 / 运行应用", color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_text(mx + 2, my + 5, "  U     - 卸载已安装应用", color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_text(mx + 2, my + 6, "  R     - 运行已安装应用", color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_text(mx + 2, my + 7, "  I     - 安装应用", color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_text(mx + 2, my + 8, "  Tab   - 切换到详情面板", color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_text(mx + 2, my + 9, "  F1    - 显示此帮助", color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_text(mx + 2, my + 10, "  Q/Esc - 退出应用中心", color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    while (keyboard_poll() == 0 && keyboard_getchar_nonblock() == 0)
        __asm__ volatile("hlt");

    for (int row = 0; row < mh; row++)
        for (int col = 0; col < mw; col++)
            VGA_BUF[(my + row) * VGA_WIDTH + (mx + col)] = saved[row * mw + col];
}

/* ---- 绘制详情面板 ---- */
static void draw_detail(app_t *app)
{
    int dx = 42, dy = 2, dw = 37, dh = 18;

    vga_fill(dx, dy, dw, dh, ' ', DETAIL_BG);
    draw_box(dx, dy, dw, dh, color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));

    vga_text(dx + 2, dy + 1, "应用详情", color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));

    if (app == NULL) {
        vga_text(dx + 2, dy + 5, "请选择一个应用", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }

    /* 图标 + 名称 */
    vga_text(dx + 2, dy + 3, "  ", DETAIL_BG);
    vga_text(dx + 2, dy + 3, app->name, color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));

    /* 状态 */
    vga_text(dx + 2, dy + 5, "状态:   ", DETAIL_LABEL);
    vga_text(dx + 10, dy + 5, state_text(app->state), state_color(app->state));

    /* 版本 */
    vga_text(dx + 2, dy + 6, "版本:   ", DETAIL_LABEL);
    vga_text(dx + 10, dy + 6, "v", DETAIL_VAL);
    vga_text(dx + 11, dy + 6, app->version, DETAIL_VAL);

    /* 大小 */
    vga_text(dx + 2, dy + 7, "大小:   ", DETAIL_LABEL);
    char size_buf[16];
    if (app->size >= 1024) {
        int kb = app->size / 1024;
        size_buf[0] = '0' + (kb / 10) % 10;
        size_buf[1] = '0' + (kb % 10);
        size_buf[2] = ' ';
        size_buf[3] = 'K';
        size_buf[4] = 'B';
        size_buf[5] = '\0';
    } else {
        size_buf[0] = '0' + (app->size / 100) % 10;
        size_buf[1] = '0' + (app->size / 10) % 10;
        size_buf[2] = '0' + (app->size % 10);
        size_buf[3] = ' ';
        size_buf[4] = 'B';
        size_buf[5] = '\0';
    }
    vga_text(dx + 10, dy + 7, size_buf, DETAIL_VAL);

    /* 描述 */
    vga_text(dx + 2, dy + 9, "描述:", DETAIL_LABEL);
    vga_text(dx + 2, dy + 10, app->desc, DETAIL_VAL);

    /* 操作提示 */
    vga_text(dx + 2, dy + 13, "───────────────", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    if (app->state == APP_STATE_INSTALLED) {
        vga_text(dx + 2, dy + 14, "  Enter/R = 运行", color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
        vga_text(dx + 2, dy + 15, "  U       = 卸载", color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    } else if (app->state == APP_STATE_RUNNING) {
        vga_text(dx + 2, dy + 14, "  应用正在运行中...", color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
    } else {
        vga_text(dx + 2, dy + 14, "  Enter/I = 安装", color(VGA_COLOR_CYAN, VGA_COLOR_BLACK));
    }
}

/* ---- 绘制应用列表 ---- */
static void draw_list(int app_count, int sel, app_t *apps)
{
    int lx = 1, ly = 2, lw = 40, lh = 20;

    vga_fill(lx, ly, lw, lh, ' ', LIST_BG);
    draw_box(lx, ly, lw, lh, color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));

    /* 列表标题 */
    vga_fill(lx + 1, ly + 1, lw - 2, 1, ' ', color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY));
    vga_text(lx + 2, ly + 1, "  应用名称              状态", color(VGA_COLOR_BLACK, VGA_COLOR_DARK_GREY));

    /* 列表项 */
    int max_show = 17;
    int start = 0;
    if (sel >= max_show) start = sel - max_show + 1;

    for (int i = 0; i < app_count && i < max_show; i++) {
        int idx = start + i;
        if (idx >= app_count) break;

        int row = ly + 2 + i;
        app_t *app = &apps[idx];

        /* 选中行高亮 */
        if (idx == sel) {
            vga_fill(lx + 1, row, lw - 2, 1, ' ', LIST_SEL_BG);
            vga_text(lx + 2, row, "[", LIST_SEL_BG);
            vga_text(lx + 3, row, "]", LIST_SEL_BG);
            vga_text(lx + 5, row, app->name, LIST_SEL_BG);
            /* 填充空格 */
            int name_len = strlen(app->name);
            for (int j = name_len; j < 24; j++)
                vga_put(lx + 5 + j, row, ' ', LIST_SEL_BG);
            vga_text(lx + 29, row, state_text(app->state), LIST_SEL_BG);
        } else {
            vga_text(lx + 2, row, " ", LIST_BG);
            vga_text(lx + 3, row, " ", LIST_BG);
            vga_text(lx + 5, row, app->name, LIST_BG);
            int name_len = strlen(app->name);
            for (int j = name_len; j < 24; j++)
                vga_put(lx + 5 + j, row, ' ', LIST_BG);
            vga_text(lx + 29, row, state_text(app->state), state_color(app->state));
        }
    }

    /* 滚动指示器 */
    if (start > 0) {
        vga_text(lx + lw - 3, ly + 1, "▲", color(VGA_COLOR_YELLOW, VGA_COLOR_DARK_GREY));
    }
    if (start + max_show < app_count) {
        vga_text(lx + lw - 3, ly + lh - 2, "▼", color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
    }
}

/* ---- 刷新整个界面 ---- */
static void refresh_ui(int app_count, int sel, app_t *apps, const char *status, uint8_t st_clr)
{
    draw_list(app_count, sel, apps);
    draw_detail((sel >= 0 && sel < app_count) ? &apps[sel] : NULL);
    draw_status_line(status, st_clr);
    terminal_update_cursor();
}

/* ---- 应用中心入口 ---- */
void app_center_main(void)
{
    int num_apps = apps_count();
    app_t *apps;

    /* 分配应用列表内存 */
    apps = (app_t *)malloc(sizeof(app_t) * num_apps);
    if (apps == NULL) {
        terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_print("错误: 内存不足，无法启动应用中心\n");
        return;
    }

    /* 读取所有应用信息 */
    for (int i = 0; i < num_apps; i++) {
        apps_get_by_index(i, &apps[i]);
    }

    /* 绘制初始界面 */
    vga_fill(0, 0, 80, 25, ' ', BG_CLR);

    /* 标题栏 */
    vga_fill(0, 0, 80, 1, ' ', TITLE_BG);
    vga_text(1, 0, "  HU basic 应用中心", TITLE_CLR);
    vga_text(30, 0, "v1.1", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));
    vga_text(55, 0, "共 ", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));
    char cnt_buf[4];
    cnt_buf[0] = '0' + (num_apps / 10) % 10;
    cnt_buf[1] = '0' + (num_apps % 10);
    cnt_buf[2] = ' ';
    cnt_buf[3] = '\0';
    vga_text(57, 0, cnt_buf, color(VGA_COLOR_YELLOW, VGA_COLOR_DARK_GREY));
    vga_text(59, 0, "个应用", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));

    /* 底部帮助栏 */
    vga_fill(0, 23, 80, 1, ' ', HELP_BG);
    vga_text(1, 23, "  ↑↓=选择(循环)  Enter=安装/运行  U=卸载  R=运行  I=安装  F1=帮助  Q=退出",
             color(VGA_COLOR_BLACK, VGA_COLOR_DARK_GREY));

    int sel = 0;
    const char *status_msg = NULL;
    uint8_t status_clr = MSG_OK;

    refresh_ui(num_apps, sel, apps, "欢迎使用应用中心! 选择一个应用开始操作", MSG_OK);

    /* 主循环 */
    while (1) {
        /* ---- 鼠标处理 ---- */
        int clicked = mouse_left_click();
        int moved = mouse_moved();
        if (moved || clicked) {
            mouse_hide();
            int mx = mouse_get_x();
            int my = mouse_get_y();

            /* 列表区域: x=1..40, y=4..20 (列表项从 ly+2 开始) */
            int lx = 1, ly = 2;
            int max_show = 17;
            int start = 0;
            if (sel >= max_show) start = sel - max_show + 1;

            if (mx >= lx && mx < lx + 40 && my >= ly + 2 && my < ly + 2 + max_show) {
                int item_idx = start + (my - (ly + 2));
                if (item_idx >= 0 && item_idx < num_apps) {
                    if (sel != item_idx) {
                        sel = item_idx;
                        refresh_ui(num_apps, sel, apps, NULL, MSG_OK);
                    }
                }
            }

            if (clicked) {
                mouse_tap_feedback();
                /* 点击列表项 → 智能操作 */
                if (mx >= lx && mx < lx + 40 && my >= ly + 2 && my < ly + 2 + max_show) {
                    int item_idx = start + (my - (ly + 2));
                    if (item_idx >= 0 && item_idx < num_apps) {
                        app_t *app = &apps[item_idx];
                        if (app->state == APP_STATE_NOT_INSTALLED) {
                            int ret = apps_install(app->name);
                            if (ret == 0) {
                                app->state = APP_STATE_INSTALLED;
                                status_msg = "安装成功! 再点击运行";
                                status_clr = MSG_OK;
                            } else {
                                status_msg = "安装失败";
                                status_clr = MSG_ERR;
                            }
                        } else if (app->state == APP_STATE_INSTALLED) {
                            uint16_t *saved = (uint16_t *)malloc(80 * 25 * 2);
                            if (saved) {
                                for (int i = 0; i < 80 * 25; i++)
                                    saved[i] = VGA_BUF[i];
                            }
                            terminal_clear();
                            apps_run(app->name);
                            if (saved) {
                                for (int i = 0; i < 80 * 25; i++)
                                    VGA_BUF[i] = saved[i];
                                free(saved);
                            }
                            apps_get_by_index(item_idx, &apps[item_idx]);
                            status_msg = "应用已退出";
                            status_clr = MSG_OK;
                        }
                        refresh_ui(num_apps, sel, apps, status_msg, status_clr);
                    }
                }
                /* 点击底部栏 → 退出 */
                if (my >= 23) {
                    free(apps);
                    terminal_clear();
                    return;
                }
            }
            mouse_draw();
        }

        int key = keyboard_poll();

        if (key == KEY_NONE) {
            char c = keyboard_getchar_nonblock();
            if (c == 0) {
                __asm__ volatile("hlt");
                continue;
            }

            if (c == 'q' || c == 'Q') {
                free(apps);
                terminal_clear();
                return;
            }

            if (c == 'u' || c == 'U') {
                /* 卸载 */
                if (sel >= 0 && sel < num_apps) {
                    app_t *app = &apps[sel];
                    if (app->state == APP_STATE_INSTALLED) {
                        int ret = apps_uninstall(app->name);
                        if (ret == 0) {
                            app->state = APP_STATE_NOT_INSTALLED;
                            status_msg = "应用已卸载";
                            status_clr = MSG_OK;
                        } else {
                            status_msg = "卸载失败";
                            status_clr = MSG_ERR;
                        }
                    } else {
                        status_msg = "该应用未安装，无需卸载";
                        status_clr = MSG_WARN;
                    }
                }
                refresh_ui(num_apps, sel, apps, status_msg, status_clr);
                continue;
            }

            if (c == 'r' || c == 'R') {
                /* 运行 */
                if (sel >= 0 && sel < num_apps) {
                    app_t *app = &apps[sel];
                    if (app->state == APP_STATE_INSTALLED) {
                        /* 保存屏幕 */
                        uint16_t *saved = (uint16_t *)malloc(80 * 25 * 2);
                        if (saved) {
                            for (int i = 0; i < 80 * 25; i++)
                                saved[i] = VGA_BUF[i];
                        }

                        terminal_clear();
                        apps_run(app->name);

                        /* 恢复屏幕 */
                        if (saved) {
                            for (int i = 0; i < 80 * 25; i++)
                                VGA_BUF[i] = saved[i];
                            free(saved);
                        }

                        /* 重新读取状态 */
                        apps_get_by_index(sel, &apps[sel]);
                        status_msg = "应用已退出";
                        status_clr = MSG_OK;
                        refresh_ui(num_apps, sel, apps, status_msg, status_clr);
                    } else {
                        status_msg = "请先安装该应用";
                        status_clr = MSG_WARN;
                        refresh_ui(num_apps, sel, apps, status_msg, status_clr);
                    }
                }
                continue;
            }

            if (c == 'i' || c == 'I') {
                /* 安装 */
                if (sel >= 0 && sel < num_apps) {
                    app_t *app = &apps[sel];
                    if (app->state == APP_STATE_NOT_INSTALLED) {
                        int ret = apps_install(app->name);
                        if (ret == 0) {
                            app->state = APP_STATE_INSTALLED;
                            status_msg = "应用安装成功!";
                            status_clr = MSG_OK;
                        } else if (ret == -2) {
                            app->state = APP_STATE_INSTALLED;
                            status_msg = "应用已安装";
                            status_clr = MSG_WARN;
                        } else {
                            status_msg = "安装失败";
                            status_clr = MSG_ERR;
                        }
                    } else {
                        status_msg = "应用已安装";
                        status_clr = MSG_WARN;
                    }
                }
                refresh_ui(num_apps, sel, apps, status_msg, status_clr);
                continue;
            }

            continue;
        }

        /* 特殊键处理 */
        if (key == KEY_UP) {
            if (sel > 0) {
                sel--;
            } else {
                /* 顶部环绕: 跳到最后一个 */
                sel = num_apps - 1;
            }
            refresh_ui(num_apps, sel, apps, NULL, MSG_OK);
        } else if (key == KEY_DOWN) {
            if (sel < num_apps - 1) {
                sel++;
            } else {
                /* 底部环绕: 跳到第一个 */
                sel = 0;
            }
            refresh_ui(num_apps, sel, apps, NULL, MSG_OK);
        } else if (key == KEY_ENTER) {
            /* 智能操作: 未安装→安装, 已安装→运行 */
            if (sel >= 0 && sel < num_apps) {
                app_t *app = &apps[sel];
                if (app->state == APP_STATE_NOT_INSTALLED) {
                    int ret = apps_install(app->name);
                    if (ret == 0) {
                        app->state = APP_STATE_INSTALLED;
                        status_msg = "安装成功! 再按 Enter 运行";
                        status_clr = MSG_OK;
                    } else {
                        status_msg = "安装失败";
                        status_clr = MSG_ERR;
                    }
                    refresh_ui(num_apps, sel, apps, status_msg, status_clr);
                } else if (app->state == APP_STATE_INSTALLED) {
                    /* 保存屏幕 */
                    uint16_t *saved = (uint16_t *)malloc(80 * 25 * 2);
                    if (saved) {
                        for (int i = 0; i < 80 * 25; i++)
                            saved[i] = VGA_BUF[i];
                    }

                    terminal_clear();
                    apps_run(app->name);

                    if (saved) {
                        for (int i = 0; i < 80 * 25; i++)
                            VGA_BUF[i] = saved[i];
                        free(saved);
                    }

                    apps_get_by_index(sel, &apps[sel]);
                    refresh_ui(num_apps, sel, apps, "应用已退出", MSG_OK);
                }
            }
        } else if (key == KEY_ESC) {
            free(apps);
            terminal_clear();
            return;
        } else if (key == KEY_F1) {
            show_help();
            /* 恢复界面 */
            vga_fill(0, 0, 80, 25, ' ', BG_CLR);
            vga_fill(0, 0, 80, 1, ' ', TITLE_BG);
            vga_text(1, 0, "  HU basic 应用中心", TITLE_CLR);
            vga_text(30, 0, "v1.1", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));
            vga_text(55, 0, "共 ", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));
            vga_text(57, 0, cnt_buf, color(VGA_COLOR_YELLOW, VGA_COLOR_DARK_GREY));
            vga_text(59, 0, "个应用", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));
            vga_fill(0, 23, 80, 1, ' ', HELP_BG);
            vga_text(1, 23, "  ↑↓=选择  Enter=安装/运行  U=卸载  R=运行  I=安装  Tab=详情  F1=帮助  Q=退出",
                     color(VGA_COLOR_BLACK, VGA_COLOR_DARK_GREY));
            refresh_ui(num_apps, sel, apps, NULL, MSG_OK);
        }
    }
}