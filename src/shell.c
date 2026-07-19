/*
 * shell.c - 简单交互式 Shell 实现
 * 支持命令: help, clear, echo, time, mem, about, reboot,
 *           apps, install, run, uninstall
 */
#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include "memory.h"
#include "string.h"
#include "ports.h"
#include "apps.h"

/* 命令缓冲区 */
#define CMD_BUFFER_SIZE 256
#define PROMPT          "kernel> "

/* 命令处理函数类型 */
typedef void (*cmd_func_t)(const char *args);

/* 命令表 */
typedef struct {
    const char *name;
    const char *desc;
    cmd_func_t func;
} command_t;

/* 前向声明 */
static void cmd_help(const char *args);
static void cmd_clear(const char *args);
static void cmd_echo(const char *args);
static void cmd_time(const char *args);
static void cmd_mem(const char *args);
static void cmd_about(const char *args);
static void cmd_reboot(const char *args);
static void cmd_apps(const char *args);
static void cmd_install(const char *args);
static void cmd_run(const char *args);
static void cmd_uninstall(const char *args);
static void cmd_desktop(const char *args);
static void cmd_browser(const char *args);
static void cmd_appcenter(const char *args);

static volatile int shell_exit = 0;

static command_t commands[] = {
    {"help",      "显示帮助信息",         cmd_help},
    {"clear",     "清屏",                  cmd_clear},
    {"echo",      "回显文本",              cmd_echo},
    {"time",      "显示系统运行时间",      cmd_time},
    {"mem",       "显示内存使用情况",      cmd_mem},
    {"about",     "关于本内核",            cmd_about},
    {"apps",      "列出所有可用应用",      cmd_apps},
    {"install",   "安装应用 (install <name>)", cmd_install},
    {"run",       "运行应用 (run <name>)", cmd_run},
    {"uninstall", "卸载应用 (uninstall <name>)", cmd_uninstall},
    {"reboot",    "重启系统",              cmd_reboot},
    {"desktop",   "返回桌面环境",          cmd_desktop},
    {"browser",   "basic browser 网页浏览器", cmd_browser},
    {"appcenter", "应用中心 - 管理所有应用",  cmd_appcenter},
    {NULL,        NULL,                    NULL}
};

void shell_init(void)
{
    shell_exit = 0;
    terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_print("============================================\n");
    terminal_print("       Basic Kernel - Interactive Shell\n");
    terminal_print("============================================\n");
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_print("输入 'help' 查看可用命令\n");
    terminal_print("输入 'apps' 查看可安装的应用\n");
    terminal_print("输入 'desktop' 返回桌面环境\n\n");
}

void shell_run(void)
{
    char buffer[CMD_BUFFER_SIZE];

    while (!shell_exit) {
        terminal_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_print(PROMPT);
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

        keyboard_readline(buffer, CMD_BUFFER_SIZE);

        /* 跳过空行 */
        if (buffer[0] == '\0') continue;

        /* 分离命令名和参数 */
        char *cmd_name = buffer;
        char *args = buffer;

        /* 跳过命令名 */
        while (*args && !isspace(*args)) args++;

        if (*args) {
            *args = '\0';
            args++;
            /* 跳过空格 */
            while (*args && isspace(*args)) args++;
        }

        /* 查找并执行命令 */
        int found = 0;
        for (int i = 0; commands[i].name != NULL; i++) {
            if (strcmp(cmd_name, commands[i].name) == 0) {
                commands[i].func(args);
                found = 1;
                break;
            }
        }

        if (!found) {
            terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            terminal_print("未知命令: ");
            terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            terminal_print(cmd_name);
            terminal_print("\n输入 'help' 查看可用命令\n");
        }
    }
}

static void cmd_help(const char *args)
{
    (void)args;
    terminal_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_print("可用命令:\n");
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (int i = 0; commands[i].name != NULL; i++) {
        terminal_print("  ");
        terminal_print(commands[i].name);
        /* 对齐 */
        int len = strlen(commands[i].name);
        for (int j = len; j < 10; j++) {
            terminal_putchar(' ');
        }
        terminal_print("- ");
        terminal_print(commands[i].desc);
        terminal_putchar('\n');
    }
    terminal_print("\n示例: install calculator, run calculator\n");
}

static void cmd_clear(const char *args)
{
    (void)args;
    terminal_clear();
}

static void cmd_echo(const char *args)
{
    terminal_print(args);
    terminal_putchar('\n');
}

static void cmd_time(const char *args)
{
    (void)args;
    terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_print("系统运行时间: ");
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    uint32_t seconds = timer_get_seconds();
    uint32_t hours   = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs    = seconds % 60;

    terminal_print_dec(hours);
    terminal_print(" 小时 ");
    terminal_print_dec(minutes);
    terminal_print(" 分钟 ");
    terminal_print_dec(secs);
    terminal_print(" 秒 (");
    terminal_print_dec(timer_get_ticks());
    terminal_print(" ticks)\n");
}

static void cmd_mem(const char *args)
{
    (void)args;
    uint32_t total, used, free;
    memory_info(&total, &used, &free);

    terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_print("内存信息:\n");
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_print("  总大小: ");
    terminal_print_dec(total / 1024);
    terminal_print(" KB\n");
    terminal_print("  已使用: ");
    terminal_print_dec(used / 1024);
    terminal_print(" KB\n");
    terminal_print("  可用:   ");
    terminal_print_dec(free / 1024);
    terminal_print(" KB\n");
}

static void cmd_about(const char *args)
{
    (void)args;
    terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_print("Basic Kernel v1.1\n");
    terminal_print("=================\n");
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_print("一个用于学习的 x86 32-bit 操作系统内核\n");
    terminal_print("特性:\n");
    terminal_print("  - Multiboot 引导\n");
    terminal_print("  - GDT / IDT 中断管理\n");
    terminal_print("  - VGA 文本模式显示\n");
    terminal_print("  - PS/2 键盘驱动\n");
    terminal_print("  - PIT 定时器\n");
    terminal_print("  - 基础内存管理\n");
    terminal_print("  - 应用安装程序\n");
    terminal_print("  - 交互式 Shell\n");
}

static void cmd_reboot(const char *args)
{
    (void)args;
    terminal_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_print("正在重启...\n\n");

    uint8_t status;
    do {
        status = inb(0x64);
    } while (status & 0x02);

    outb(0x64, 0xFE);

    __asm__ volatile("int $0");
}

/* ---- 应用管理命令 ---- */

static void cmd_apps(const char *args)
{
    (void)args;
    terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_print("应用商店 (");
    terminal_print_dec(apps_count());
    terminal_print(" 个应用):\n");
    terminal_print("----------------------------------------------\n");
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_print("  名称           描述                        状态\n");
    terminal_print("----------------------------------------------\n");

    char list_buf[2048];
    apps_list_all(list_buf, sizeof(list_buf));
    terminal_print(list_buf);

    terminal_print("----------------------------------------------\n");
    terminal_print("使用 'install <名称>' 安装, 'run <名称>' 运行\n");
}

static void cmd_install(const char *args)
{
    if (args[0] == '\0') {
        terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_print("用法: install <应用名称>\n");
        terminal_print("输入 'apps' 查看可用应用\n");
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        return;
    }

    /* 提取应用名 (去掉可能的参数) */
    char app_name[32];
    strncpy(app_name, args, 31);
    app_name[31] = '\0';
    /* 截断到第一个空格 */
    for (int i = 0; app_name[i]; i++) {
        if (isspace(app_name[i])) {
            app_name[i] = '\0';
            break;
        }
    }

    int ret = apps_install(app_name);
    if (ret == 0) {
        terminal_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
        terminal_print("✓ 应用 '");
        terminal_print(app_name);
        terminal_print("' 安装成功!\n");
        terminal_print("  输入 'run ");
        terminal_print(app_name);
        terminal_print("' 来启动\n");
    } else if (ret == -2) {
        terminal_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        terminal_print("! 应用 '");
        terminal_print(app_name);
        terminal_print("' 已经安装过了\n");
    } else {
        terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_print("✗ 未找到应用 '");
        terminal_print(app_name);
        terminal_print("'\n");
        terminal_print("  输入 'apps' 查看可用应用列表\n");
    }
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void cmd_browser(const char *args)
{
    (void)args;
    /* 通过 app manager 运行 browser */
    int ret = apps_run("browser");
    if (ret == -2) {
        terminal_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        terminal_print("! basic browser 尚未安装，请先运行: install browser\n");
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }
}

static void cmd_appcenter(const char *args)
{
    (void)args;
    int ret = apps_run("appcenter");
    if (ret == -2) {
        terminal_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        terminal_print("! 应用中心尚未安装，请先运行: install appcenter\n");
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }
}

static void cmd_desktop(const char *args)
{
    (void)args;
    terminal_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_print("返回桌面...\n");
    shell_exit = 1;
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void shell_request_exit(void)
{
    shell_exit = 1;
}

static void cmd_run(const char *args)
{
    if (args[0] == '\0') {
        terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_print("用法: run <应用名称>\n");
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        return;
    }

    char app_name[32];
    strncpy(app_name, args, 31);
    app_name[31] = '\0';
    for (int i = 0; app_name[i]; i++) {
        if (isspace(app_name[i])) {
            app_name[i] = '\0';
            break;
        }
    }

    int ret = apps_run(app_name);
    if (ret == 0) {
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        /* 应用已运行并退出 */
    } else if (ret == -2) {
        terminal_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        terminal_print("! 应用 '");
        terminal_print(app_name);
        terminal_print("' 尚未安装\n");
        terminal_print("  输入 'install ");
        terminal_print(app_name);
        terminal_print("' 先安装\n");
    } else {
        terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_print("✗ 未找到应用 '");
        terminal_print(app_name);
        terminal_print("'\n");
    }
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void cmd_uninstall(const char *args)
{
    if (args[0] == '\0') {
        terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_print("用法: uninstall <应用名称>\n");
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        return;
    }

    char app_name[32];
    strncpy(app_name, args, 31);
    app_name[31] = '\0';
    for (int i = 0; app_name[i]; i++) {
        if (isspace(app_name[i])) {
            app_name[i] = '\0';
            break;
        }
    }

    int ret = apps_uninstall(app_name);
    if (ret == 0) {
        terminal_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
        terminal_print("✓ 应用 '");
        terminal_print(app_name);
        terminal_print("' 已卸载\n");
    } else if (ret == -2) {
        terminal_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        terminal_print("! 应用 '");
        terminal_print(app_name);
        terminal_print("' 本来就未安装\n");
    } else {
        terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_print("✗ 未找到应用 '");
        terminal_print(app_name);
        terminal_print("'\n");
    }
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}