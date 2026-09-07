/*
 * browser.c - Basic Browser v2.0
 * 现代文本模式浏览器: 标签页、地址栏、书签、历史记录、页面渲染
 */
#include "apps.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include "string.h"
#include "memory.h"
#include "version.h"
#include "net/network.h"
#include "net/rtl8139.h"

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

#define URL_MAX   256
#define RESP_MAX  65536
#define TAB_MAX   8

/* 颜色 */
#define BG_CLR       color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK)
#define TOOLBAR_BG   color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY)
#define TAB_BG       color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY)
#define TAB_ACTIVE   color(VGA_COLOR_BLACK, VGA_COLOR_WHITE)
#define TAB_INACTIVE color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY)
#define URL_BG       color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY)
#define URL_CLR      color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY)
#define CONTENT_BG   color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK)
#define STATUS_BG    color(VGA_COLOR_WHITE, VGA_COLOR_BLUE)
#define HEADER_CLR   color(VGA_COLOR_CYAN, VGA_COLOR_BLACK)
#define LINK_CLR     color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK)
#define ACCENT_CLR   color(VGA_COLOR_GREEN, VGA_COLOR_BLACK)
#define WARN_CLR     color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK)
#define MUTED_CLR    color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK)
#define VALUE_CLR    color(VGA_COLOR_WHITE, VGA_COLOR_BLACK)

/* 标签页 */
typedef struct {
    char url[URL_MAX];
    char title[64];
    int  active;
} tab_t;

static tab_t tabs[TAB_MAX];
static int tab_count = 1;
static int active_tab = 0;
static char url_buffer[URL_MAX];

/* 书签 */
static const char *bookmarks[] = {
    "http://example.com",
    "http://httpbin.org/headers",
    "http://httpbin.org/ip",
    "https://github.com/HuHuBasic",
    NULL
};

/* 历史记录 */
static char history[16][URL_MAX];
static int history_count = 0;
static int history_pos = -1;

static void add_history(const char *url)
{
    if (history_count < 16) {
        strncpy(history[history_count], url, URL_MAX - 1);
        history[history_count][URL_MAX - 1] = '\0';
        history_count++;
        history_pos = history_count - 1;
    }
}

/* 绘制工具栏 */
static void draw_toolbar(void)
{
    /* 工具栏背景 */
    vga_fill(0, 0, 80, 1, ' ', TOOLBAR_BG);
    vga_text(0, 0, " Basic Browser v2.0", color(VGA_COLOR_BLACK, VGA_COLOR_DARK_GREY));
    vga_text(55, 0, "F1=帮助 F5=刷新", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));
}

/* 绘制标签栏 */
static void draw_tabs(void)
{
    vga_fill(0, 1, 80, 1, ' ', TAB_BG);
    int x = 0;
    for (int i = 0; i < tab_count; i++) {
        int len = strlen(tabs[i].title) + 4;
        if (len > 20) len = 20;
        if (x + len > 78) break;

        if (i == active_tab) {
            vga_fill(x, 1, len, 1, ' ', TAB_ACTIVE);
            vga_text(x + 1, 1, " ", TAB_ACTIVE);
            vga_text_n(x + 2, 1, tabs[i].title, len - 4, TAB_ACTIVE);
            vga_text(x + len - 2, 1, " x", TAB_ACTIVE);
        } else {
            vga_text(x + 1, 1, " ", TAB_INACTIVE);
            vga_text_n(x + 2, 1, tabs[i].title, len - 4, TAB_INACTIVE);
            vga_text(x + len - 2, 1, " x", TAB_INACTIVE);
        }
        x += len;
    }
    /* 新标签按钮 */
    if (x < 78) {
        vga_text(x + 1, 1, "[+]", TAB_INACTIVE);
    }
}

/* 绘制地址栏 */
static void draw_url_bar(void)
{
    vga_fill(0, 2, 80, 1, ' ', URL_BG);
    vga_text(1, 2, " ", URL_BG);
    if (tabs[active_tab].url[0]) {
        vga_text(2, 2, tabs[active_tab].url, URL_CLR);
    } else {
        vga_text(2, 2, "输入 URL 或书签编号...", MUTED_CLR);
    }
}

/* 绘制书签栏 */
static void draw_bookmarks_bar(void)
{
    vga_fill(0, 3, 80, 1, ' ', color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_DARK_GREY));
    vga_text(1, 3, "书签:", color(VGA_COLOR_BLACK, VGA_COLOR_DARK_GREY));
    int x = 7;
    for (int i = 0; bookmarks[i]; i++) {
        char label[8];
        label[0] = '1' + i;
        label[1] = ':';
        label[2] = ' ';
        int blen = strlen(bookmarks[i]);
        if (blen > 18) blen = 18;
        for (int j = 0; j < blen && j < 15; j++) label[3 + j] = bookmarks[i][j];
        label[3 + blen] = '\0';
        if (x + blen + 2 < 78) {
            vga_text(x, 3, label, color(VGA_COLOR_BLACK, VGA_COLOR_DARK_GREY));
            x += blen + 5;
        }
    }
}

/* 解析 HTML 文本 */
static void print_html_text(const char *html, int len)
{
    int i = 0, col = 0, row = 4;
    while (i < len && row < 22) {
        if (html[i] == '<') {
            while (i < len && html[i] != '>') i++;
            if (i < len) i++;
            if (col > 0) { vga_put(col, row, '\n', CONTENT_BG); col = 0; row++; }
            continue;
        }
        if (strncmp(html + i, "&nbsp;", 6) == 0) { vga_put(col, row, ' ', CONTENT_BG); col++; i += 6; continue; }
        if (strncmp(html + i, "&lt;", 4) == 0) { vga_put(col, row, '<', CONTENT_BG); col++; i += 4; continue; }
        if (strncmp(html + i, "&gt;", 4) == 0) { vga_put(col, row, '>', CONTENT_BG); col++; i += 4; continue; }
        if (strncmp(html + i, "&amp;", 5) == 0) { vga_put(col, row, '&', CONTENT_BG); col++; i += 5; continue; }
        if (strncmp(html + i, "&quot;", 6) == 0) { vga_put(col, row, '"', CONTENT_BG); col++; i += 6; continue; }
        if (html[i] == '&') { while (i < len && html[i] != ';') i++; if (i < len) i++; continue; }
        if (html[i] == '\n' || html[i] == '\r') { i++; continue; }
        if (html[i] == ' ' || html[i] == '\t') {
            if (col < 78) { vga_put(col, row, ' ', CONTENT_BG); col++; }
            while (i < len && (html[i] == ' ' || html[i] == '\t')) i++;
            continue;
        }
        if (col >= 78) { col = 0; row++; }
        vga_put(col, row, html[i], CONTENT_BG);
        col++; i++;
    }
}

/* 渲染页面 */
static void render_page(const char *response, int len)
{
    vga_fill(0, 4, 80, 19, ' ', CONTENT_BG);
    if (len <= 0) {
        vga_text(2, 6, "(空响应或无法加载页面)", MUTED_CLR);
        return;
    }
    /* 显示 HTTP 响应头 */
    vga_text(2, 4, "--- 响应 ---", HEADER_CLR);
    print_html_text(response, len);
}

/* 绘制主界面 */
static void draw_browser_ui(void)
{
    draw_toolbar();
    draw_tabs();
    draw_url_bar();
    draw_bookmarks_bar();
    /* 底部状态栏 */
    vga_fill(0, 23, 80, 1, ' ', STATUS_BG);
    vga_text(1, 23, "Basic Browser v2.0", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLUE));
    vga_text(60, 23, "Q=退出", color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLUE));
}

/* 帮助弹窗 */
static void show_browser_help(void)
{
    uint16_t saved[80 * 25];
    for (int i = 0; i < 80 * 25; i++) saved[i] = VGA_BUF[i];

    vga_fill(0, 0, 80, 25, ' ', color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_text(2, 2, "Basic Browser v2.0 — 帮助", HEADER_CLR);
    vga_text(2, 4, "输入 URL 地址打开网页", VALUE_CLR);
    vga_text(2, 5, "输入 1-4 打开对应书签", VALUE_CLR);
    vga_text(2, 6, "Tab — 切换标签页", VALUE_CLR);
    vga_text(2, 7, "Ctrl+T — 新建标签页", VALUE_CLR);
    vga_text(2, 8, "Ctrl+W — 关闭标签页", VALUE_CLR);
    vga_text(2, 9, "F5 — 刷新页面", VALUE_CLR);
    vga_text(2, 10, "Alt+Left/Right — 前进/后退", VALUE_CLR);
    vga_text(2, 11, "Q — 退出浏览器", VALUE_CLR);
    vga_text(2, 13, "按任意键返回...", MUTED_CLR);

    while (keyboard_poll() == 0 && keyboard_getchar_nonblock() == 0)
        __asm__ volatile("hlt");

    for (int i = 0; i < 80 * 25; i++) VGA_BUF[i] = saved[i];
}

/* 浏览器入口 */
void app_browser(void)
{
    char *response = (char *)malloc(RESP_MAX);
    if (!response) {
        terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_print("内存不足，无法启动浏览器\n");
        return;
    }

    /* 初始化标签 */
    tab_count = 1;
    active_tab = 0;
    strncpy(tabs[0].url, "", URL_MAX - 1);
    strncpy(tabs[0].title, "新标签页", 63);
    tabs[0].active = 1;

    vga_fill(0, 0, 80, 25, ' ', BG_CLR);
    draw_browser_ui();
    vga_fill(0, 4, 80, 19, ' ', CONTENT_BG);

    /* 首页 */
    vga_text(25, 7, "Basic Browser v2.0", HEADER_CLR);
    vga_text(22, 9, "HU basic OS 内置浏览器", MUTED_CLR);
    vga_text(18, 11, "输入 URL 地址开始浏览网页", VALUE_CLR);
    vga_text(18, 12, "或输入 1-4 打开书签", MUTED_CLR);

    int running = 1;
    while (running) {
        int key = keyboard_poll();
        if (key == KEY_NONE) {
            char c = keyboard_getchar_nonblock();
            if (c == 0) { __asm__ volatile("hlt"); continue; }

            if (c == 'q' || c == 'Q') {
                running = 0;
            } else if (c == '\n' || c == '\r') {
                /* 导航到 URL */
                if (url_buffer[0] == '\0') continue;
                strncpy(tabs[active_tab].url, url_buffer, URL_MAX - 1);
                tabs[active_tab].url[URL_MAX - 1] = '\0';
                strncpy(tabs[active_tab].title, url_buffer, 63);
                tabs[active_tab].title[63] = '\0';

                url_buffer[0] = '\0';

                /* 检查书签编号 */
                const char *target_url = tabs[active_tab].url;
                if (isdigit(target_url[0]) && !target_url[1]) {
                    int idx = simple_atoi(target_url) - 1;
                    if (idx >= 0 && bookmarks[idx]) {
                        target_url = bookmarks[idx];
                        strncpy(tabs[active_tab].url, target_url, URL_MAX - 1);
                    }
                }

                vga_text(2, 5, "正在加载: ", MUTED_CLR);
                vga_text(14, 5, target_url, ACCENT_CLR);
                vga_text(2, 6, "请稍候...", MUTED_CLR);

                memset(response, 0, RESP_MAX);
                int resp_len = http_get(target_url, response, RESP_MAX);

                add_history(target_url);

                if (resp_len < 0) {
                    vga_fill(0, 4, 80, 19, ' ', CONTENT_BG);
                    vga_text(2, 6, "错误: 无法加载页面", WARN_CLR);
                    switch (resp_len) {
                        case -1: vga_text(2, 7, "无效的 URL", MUTED_CLR); break;
                        case -2: vga_text(2, 7, "DNS 解析失败", MUTED_CLR); break;
                        case -3: vga_text(2, 7, "连接失败", MUTED_CLR); break;
                        default: vga_text(2, 7, "未知错误", MUTED_CLR); break;
                    }
                } else {
                    vga_text(2, 5, "已接收 ", ACCENT_CLR);
                    char size_buf[16];
                    int sz = resp_len;
                    size_buf[0] = '0' + (sz / 10000) % 10;
                    size_buf[1] = '0' + (sz / 1000) % 10;
                    size_buf[2] = '0' + (sz / 100) % 10;
                    size_buf[3] = '0' + (sz / 10) % 10;
                    size_buf[4] = '0' + (sz % 10);
                    size_buf[5] = '\0';
                    vga_text(8, 5, size_buf, ACCENT_CLR);
                    vga_text(13, 5, " 字节", ACCENT_CLR);
                    render_page(response, resp_len);
                }
                draw_browser_ui();
            } else if (c == '\b') {
                int len = strlen(url_buffer);
                if (len > 0) url_buffer[len - 1] = '\0';
                draw_url_bar();
                vga_text(2 + len, 2, " ", URL_CLR);
            } else if (c >= ' ' && c < 127) {
                int len = strlen(url_buffer);
                if (len < URL_MAX - 1) {
                    url_buffer[len] = c;
                    url_buffer[len + 1] = '\0';
                    vga_text(2 + len, 2, &url_buffer[len], URL_CLR);
                }
            } else if (c == '\t') {
                /* Tab 切换标签 */
                active_tab = (active_tab + 1) % tab_count;
                url_buffer[0] = '\0';
                draw_browser_ui();
                vga_fill(0, 4, 80, 19, ' ', CONTENT_BG);
            } else if (c == 23) { /* Ctrl+W */
                if (tab_count > 1) {
                    for (int i = active_tab; i < tab_count - 1; i++)
                        tabs[i] = tabs[i + 1];
                    tab_count--;
                    if (active_tab >= tab_count) active_tab = tab_count - 1;
                    url_buffer[0] = '\0';
                    draw_browser_ui();
                    vga_fill(0, 4, 80, 19, ' ', CONTENT_BG);
                }
            } else if (c == 20) { /* Ctrl+T */
                if (tab_count < TAB_MAX) {
                    tab_count++;
                    active_tab = tab_count - 1;
                    strncpy(tabs[active_tab].url, "", URL_MAX - 1);
                    strncpy(tabs[active_tab].title, "新标签页", 63);
                    url_buffer[0] = '\0';
                    draw_browser_ui();
                    vga_fill(0, 4, 80, 19, ' ', CONTENT_BG);
                }
            }
            continue;
        }

        if (key == KEY_F1) {
            show_browser_help();
            draw_browser_ui();
            draw_url_bar();
            vga_fill(0, 4, 80, 19, ' ', CONTENT_BG);
        } else if (key == KEY_F5) {
            /* 刷新 */
            if (tabs[active_tab].url[0]) {
                vga_text(2, 5, "正在刷新...", MUTED_CLR);
                memset(response, 0, RESP_MAX);
                int resp_len = http_get(tabs[active_tab].url, response, RESP_MAX);
                if (resp_len >= 0) render_page(response, resp_len);
                else vga_text(2, 6, "刷新失败", WARN_CLR);
                draw_browser_ui();
            }
        } else if (key == KEY_ESC) {
            running = 0;
        } else if (key == KEY_LEFT) {
            /* 后退 */
            if (history_pos > 0) {
                history_pos--;
                strncpy(tabs[active_tab].url, history[history_pos], URL_MAX - 1);
                url_buffer[0] = '\0';
                draw_browser_ui();
                vga_text(2, 5, "正在加载...", MUTED_CLR);
                memset(response, 0, RESP_MAX);
                int resp_len = http_get(tabs[active_tab].url, response, RESP_MAX);
                if (resp_len >= 0) render_page(response, resp_len);
                draw_browser_ui();
            }
        }
    }

    free(response);
    terminal_clear();
}