/*
 * browser.c - basic browser 应用
 * 文本模式网页浏览器, 支持 HTTP GET 请求
 */
#include "apps.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include "string.h"
#include "memory.h"
#include "net/network.h"
#include "net/rtl8139.h"

#define BROWSER_URL_MAX  256
#define BROWSER_RESP_MAX 32768

/* 预置书签 */
static const char *bookmarks[] = {
    "http://example.com",
    "http://httpbin.org/headers",
    "http://httpbin.org/ip",
    NULL
};

/* 解析 HTML 实体 */
static void print_html_text(const char *html, int len)
{
    int i = 0;
    while (i < len) {
        /* 跳过标签 */
        if (html[i] == '<') {
            while (i < len && html[i] != '>') i++;
            if (i < len) i++;
            continue;
        }
        /* 处理 &nbsp; */
        if (strncmp(html + i, "&nbsp;", 6) == 0) {
            terminal_putchar(' ');
            i += 6;
            continue;
        }
        if (strncmp(html + i, "&lt;", 4) == 0) {
            terminal_putchar('<');
            i += 4;
            continue;
        }
        if (strncmp(html + i, "&gt;", 4) == 0) {
            terminal_putchar('>');
            i += 4;
            continue;
        }
        if (strncmp(html + i, "&amp;", 5) == 0) {
            terminal_putchar('&');
            i += 5;
            continue;
        }
        if (strncmp(html + i, "&quot;", 6) == 0) {
            terminal_putchar('"');
            i += 6;
            continue;
        }
        if (html[i] == '&') {
            while (i < len && html[i] != ';') i++;
            if (i < len) i++;
            continue;
        }
        /* 折叠空白 */
        if (html[i] == '\n' || html[i] == '\r') {
            i++;
            continue;
        }
        if (html[i] == ' ' || html[i] == '\t') {
            terminal_putchar(' ');
            while (i < len && (html[i] == ' ' || html[i] == '\t')) i++;
            continue;
        }
        terminal_putchar(html[i]);
        i++;
    }
}

/* 渲染网页 */
static void render_page(const char *response, int len)
{
    terminal_clear();
    terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_print("============================================\n");
    terminal_print("         basic browser v1.0\n");
    terminal_print("============================================\n\n");
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    /* 简单渲染: 移除 HTML 标签, 显示文本 */
    if (len <= 0) {
        terminal_print("(空响应)\n");
        return;
    }

    print_html_text(response, len);
}

void app_browser(void)
{
    char url_buffer[BROWSER_URL_MAX];
    char *response = (char *)malloc(BROWSER_RESP_MAX);

    if (!response) {
        terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_print("内存不足, 无法启动浏览器\n");
        return;
    }

    int running = 1;

    while (running) {
        terminal_clear();
        terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
        terminal_print("============================================\n");
        terminal_print("         basic browser v1.0\n");
        terminal_print("============================================\n\n");
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_print("输入 URL 或书签编号:\n\n");

        terminal_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        terminal_print("书签:\n");
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        for (int i = 0; bookmarks[i]; i++) {
            terminal_putchar(' ');
            terminal_print_dec(i + 1);
            terminal_print(". ");
            terminal_print(bookmarks[i]);
            terminal_putchar('\n');
        }
        terminal_print("\n");

        terminal_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_print("URL> ");
        terminal_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

        keyboard_readline(url_buffer, BROWSER_URL_MAX);

        if (url_buffer[0] == '\0') continue;
        if (strcmp(url_buffer, "q") == 0 || strcmp(url_buffer, "quit") == 0) {
            running = 0;
            break;
        }

        /* 解析书签编号 */
        const char *target_url = url_buffer;
        if (isdigit(url_buffer[0])) {
            int idx = simple_atoi(url_buffer) - 1;
            if (idx >= 0 && bookmarks[idx]) {
                target_url = bookmarks[idx];
            }
        }

        terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
        terminal_print("\n正在获取: ");
        terminal_print(target_url);
        terminal_print("\n");
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

        memset(response, 0, BROWSER_RESP_MAX);
        int resp_len = http_get(target_url, response, BROWSER_RESP_MAX);

        if (resp_len < 0) {
            terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            terminal_print("\n错误: ");
            switch (resp_len) {
                case -1: terminal_print("无效的 URL\n"); break;
                case -2: terminal_print("DNS 解析失败\n"); break;
                case -3: terminal_print("连接失败\n"); break;
                default: terminal_print("未知错误\n"); break;
            }
            terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        } else {
            terminal_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
            terminal_print("已接收 ");
            terminal_print_dec(resp_len);
            terminal_print(" 字节\n\n");
            terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

            render_page(response, resp_len);
        }

        terminal_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        terminal_print("\n\n[Enter] 返回主菜单  [q] 退出浏览器\n");
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

        while (1) {
            char c = keyboard_getchar_nonblock();
            if (c == '\n') break;
            if (c == 'q' || c == 'Q') { running = 0; break; }
            __asm__ volatile("hlt");
        }
    }

    free(response);
    terminal_clear();
}