/*
 * apps.c - 内核应用安装程序实现
 * 管理应用的注册、安装、卸载和运行
 */
#include "apps.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include "memory.h"
#include "string.h"

/* 前向声明外部应用 */
extern void app_browser(void);
extern void app_center_main(void);
/* 全局应用注册表 */
static app_t app_registry[MAX_APPS];
static int   app_count = 0;

/* ---- 内置应用: 计算器 ---- */
static void app_calculator(void)
{
    terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_print("\n============================================\n");
    terminal_print("              简易计算器\n");
    terminal_print("============================================\n\n");
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_print("操作: + - * /  (输入 'q' 退出)\n\n");

    char buf[64];
    int pos = 0;
    int a = 0, b = 0;
    char op = 0;
    int state = 0;  /* 0=等待第一个数, 1=等待运算符, 2=等待第二个数 */

    while (1) {
        terminal_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        if (state == 0) terminal_print("calc> ");
        terminal_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

        char c = keyboard_getchar_nonblock();
        if (c == 0) {
            __asm__ volatile("hlt");
            continue;
        }

        if (c == 'q' || c == 'Q') {
            terminal_print("\n退出计算器\n");
            return;
        }

        if (c == '\n') {
            if (state == 0) {
                if (pos == 0) continue;
                buf[pos] = '\0';
                a = simple_atoi(buf);
                state = 1;
                terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
                terminal_print("\n");
                terminal_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                terminal_print("op> ");
                terminal_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                pos = 0;
            } else if (state == 1) {
                if (pos == 0) continue;
                op = buf[0];
                state = 2;
                terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
                terminal_print("\n");
                terminal_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                terminal_print("calc> ");
                terminal_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                pos = 0;
            } else if (state == 2) {
                if (pos == 0) continue;
                buf[pos] = '\0';
                b = simple_atoi(buf);

                int result = 0;
                switch (op) {
                    case '+': result = a + b; break;
                    case '-': result = a - b; break;
                    case '*': result = a * b; break;
                    case '/':
                        if (b == 0) {
                            terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                            terminal_print("\n错误: 不能除以零!\n");
                            a = 0; b = 0; op = 0; state = 0; pos = 0;
                            continue;
                        }
                        result = a / b;
                        break;
                    default:
                        terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                        terminal_print("\n错误: 无效运算符!\n");
                        a = 0; b = 0; op = 0; state = 0; pos = 0;
                        continue;
                }

                terminal_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
                terminal_print("\n  = ");
                terminal_print_dec(result);
                terminal_print("\n\n");

                /* 自动用结果继续 */
                a = result;
                state = 0;
                pos = 0;
            }
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                terminal_putchar('\b');
            }
        } else if (c >= ' ' && pos < 63) {
            buf[pos++] = c;
            terminal_putchar(c);
        }
    }
}

/* ---- 内置应用: 猜数字游戏 ---- */
static void app_guess(void)
{
    terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_print("\n============================================\n");
    terminal_print("              猜数字游戏\n");
    terminal_print("============================================\n\n");
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_print("我在 1-100 之间想了一个数字, 你来猜!\n");
    terminal_print("输入 'q' 退出\n\n");

    /* 用定时器 ticks 做伪随机种子 */
    int secret = (timer_get_ticks() % 100) + 1;
    int attempts = 0;
    char buf[16];
    int pos = 0;

    while (1) {
        terminal_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_print("guess> ");
        terminal_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

        char c = keyboard_getchar_nonblock();
        if (c == 0) {
            __asm__ volatile("hlt");
            continue;
        }
        if (c == 'q' || c == 'Q') {
            terminal_print("\n退出游戏. 答案是: ");
            terminal_print_dec(secret);
            terminal_putchar('\n');
            return;
        }
        if (c == '\n') {
            if (pos == 0) continue;
            buf[pos] = '\0';
            int guess = simple_atoi(buf);
            attempts++;
            terminal_putchar('\n');

            if (guess < secret) {
                terminal_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
                terminal_print("  太小了, 再大一点!\n");
            } else if (guess > secret) {
                terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                terminal_print("  太大了, 再小一点!\n");
            } else {
                terminal_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
                terminal_print("  恭喜你猜对了! 用了 ");
                terminal_print_dec(attempts);
                terminal_print(" 次.\n\n");
                terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
                return;
            }
            pos = 0;
        } else if (c == '\b') {
            if (pos > 0) { pos--; terminal_putchar('\b'); }
        } else if (c >= '0' && c <= '9' && pos < 15) {
            buf[pos++] = c;
            terminal_putchar(c);
        }
    }
}

/* ---- 内置应用: 系统监控 ---- */
static void app_monitor(void)
{
    uint32_t last_ticks = timer_get_ticks();

    terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_print("\n============================================\n");
    terminal_print("              系统监控器\n");
    terminal_print("============================================\n");
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_print("按任意键刷新, 按 'q' 退出\n\n");

    while (1) {
        char c = keyboard_getchar_nonblock();
        if (c == 'q' || c == 'Q') {
            terminal_print("\n退出监控器\n");
            return;
        }

        uint32_t now = timer_get_ticks();
        if (c != 0 || now - last_ticks >= 100) { /* 每秒刷新一次 */
            last_ticks = now;

            terminal_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
            terminal_print("  ┌─────────────────────────────────────────┐\n");

            terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            terminal_print("  │ 运行时间: ");
            uint32_t sec = timer_get_seconds();
            terminal_print_dec(sec / 3600);
            terminal_print("h ");
            terminal_print_dec((sec % 3600) / 60);
            terminal_print("m ");
            terminal_print_dec(sec % 60);
            terminal_print("s");
            for (int i = 0; i < 18; i++) terminal_putchar(' ');
            terminal_print("│\n");

            terminal_print("  │ Ticks: ");
            terminal_print_dec(now);
            for (int i = 0; i < 24; i++) terminal_putchar(' ');
            terminal_print("│\n");

            /* 内存信息 */
            uint32_t total, used, free;
            memory_info(&total, &used, &free);
            terminal_print("  │ 内存: ");
            terminal_print_dec(used / 1024);
            terminal_print("K / ");
            terminal_print_dec(total / 1024);
            terminal_print("K");
            for (int i = 0; i < 18; i++) terminal_putchar(' ');
            terminal_print("│\n");

            /* 已安装应用数 */
            int installed = 0;
            for (int i = 0; i < MAX_APPS; i++) {
                if (app_registry[i].state == APP_STATE_INSTALLED)
                    installed++;
            }
            terminal_print("  │ 已安装应用: ");
            terminal_print_dec(installed);
            for (int i = 0; i < 22; i++) terminal_putchar(' ');
            terminal_print("│\n");

            terminal_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
            terminal_print("  └─────────────────────────────────────────┘\n");
            terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        }

        if (c == 0) {
            __asm__ volatile("hlt");
        }
    }
}

/* ---- 内置应用: ASCII 艺术 ---- */
static void app_ascii(void)
{
    terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_print("\n============================================\n");
    terminal_print("             ASCII 艺术画廊\n");
    terminal_print("============================================\n\n");

    terminal_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_print("      ___           _         _    __\n");
    terminal_print("     / __\\         (_)       / |  / /\n");
    terminal_print("    /__\\// __ _ ___ _  ___  | | / /_\n");
    terminal_print("   / \\/  \\/ _` / __| |/ __| | |/ / _ \\\n");
    terminal_print("   \\_____/\\__,_\\__ \\_|\\__ \\ |_|_/\\___/\n");
    terminal_print("                  |__/    |__/\n");

    terminal_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    terminal_print("\n       /\\_/\\\n");
    terminal_print("      ( o.o )\n");
    terminal_print("       > ^ <\n");

    terminal_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_print("\n        *    *  ****  *    *  ****\n");
    terminal_print("        *   *  *      **   * *\n");
    terminal_print("        ****    ****   * *  * * ***\n");
    terminal_print("        *   *       *  *  * * *   *\n");
    terminal_print("        *    *  ****   *   **  ****\n");

    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_print("\n按任意键退出...\n");

    while (1) {
        if (keyboard_getchar_nonblock() != 0) break;
        __asm__ volatile("hlt");
    }
}

/* ---- 初始化应用管理器 ---- */
void apps_init(void)
{
    app_count = 0;
    memset(app_registry, 0, sizeof(app_registry));

    /* 注册内置应用 */
    apps_register("calculator", "简易计算器 (+ - * /)",       "1.0", 4096,  app_calculator);
    apps_register("guess",      "猜数字游戏 (1-100)",           "1.0", 2048,  app_guess);
    apps_register("monitor",    "系统监控面板",                 "1.0", 3072,  app_monitor);
    apps_register("ascii",      "ASCII 艺术画廊",               "1.0", 1024,  app_ascii);
    apps_register("browser",    "basic browser 网页浏览器",     "1.0", 65536, app_browser);
    apps_register("appcenter",  "应用中心 - 管理所有应用",      "1.0", 8192,  app_center_main);

    /* 自动安装浏览器和应用中心 */
    apps_install("browser");
    apps_install("appcenter");
}

int apps_register(const char *name, const char *desc, const char *version,
                  uint32_t size, app_entry_t entry)
{
    if (app_count >= MAX_APPS) return -1;

    strncpy(app_registry[app_count].name, name, APP_NAME_LEN - 1);
    strncpy(app_registry[app_count].desc, desc, APP_DESC_LEN - 1);
    strncpy(app_registry[app_count].version, version, 7);
    app_registry[app_count].size  = size;
    app_registry[app_count].entry = entry;
    app_registry[app_count].state = APP_STATE_NOT_INSTALLED;
    app_count++;
    return 0;
}

int apps_install(const char *name)
{
    for (int i = 0; i < app_count; i++) {
        if (strcmp(app_registry[i].name, name) == 0) {
            if (app_registry[i].state == APP_STATE_INSTALLED) {
                return -2; /* 已安装 */
            }
            app_registry[i].state = APP_STATE_INSTALLED;
            return 0;
        }
    }
    return -1; /* 未找到 */
}

int apps_uninstall(const char *name)
{
    for (int i = 0; i < app_count; i++) {
        if (strcmp(app_registry[i].name, name) == 0) {
            if (app_registry[i].state == APP_STATE_NOT_INSTALLED) {
                return -2; /* 未安装 */
            }
            if (app_registry[i].state == APP_STATE_RUNNING) {
                return -3; /* 正在运行 */
            }
            app_registry[i].state = APP_STATE_NOT_INSTALLED;
            return 0;
        }
    }
    return -1;
}

int apps_run(const char *name)
{
    for (int i = 0; i < app_count; i++) {
        if (strcmp(app_registry[i].name, name) == 0) {
            if (app_registry[i].state != APP_STATE_INSTALLED) {
                return -2; /* 未安装 */
            }
            app_registry[i].state = APP_STATE_RUNNING;
            app_registry[i].entry();
            app_registry[i].state = APP_STATE_INSTALLED;
            return 0;
        }
    }
    return -1;
}

int apps_list_all(char *buffer, int max_len)
{
    int pos = 0;
    for (int i = 0; i < app_count; i++) {
        pos += snprintf_simple(buffer + pos, max_len - pos,
            "  %-14s %-28s v%s  %s\n",
            app_registry[i].name,
            app_registry[i].desc,
            app_registry[i].version,
            app_registry[i].state == APP_STATE_INSTALLED ? "[已安装]" : "[未安装]");
    }
    return pos;
}

int apps_list_installed(char *buffer, int max_len)
{
    int pos = 0;
    for (int i = 0; i < app_count; i++) {
        if (app_registry[i].state == APP_STATE_INSTALLED) {
            pos += snprintf_simple(buffer + pos, max_len - pos,
                "  %-14s %-28s v%s\n",
                app_registry[i].name,
                app_registry[i].desc,
                app_registry[i].version);
        }
    }
    return pos;
}

int apps_count(void)
{
    return app_count;
}

int apps_get_by_index(int index, app_t *out)
{
    if (index < 0 || index >= app_count || out == NULL) return -1;
    memcpy(out, &app_registry[index], sizeof(app_t));
    return 0;
}

int apps_get_state(const char *name)
{
    for (int i = 0; i < app_count; i++) {
        if (strcmp(app_registry[i].name, name) == 0)
            return app_registry[i].state;
    }
    return -1;
}