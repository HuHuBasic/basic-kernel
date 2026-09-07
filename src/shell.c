/* shell.c — 增强 Shell: 支持文件操作 */

#include "kernel.h"

#define INPUT_BUF_SIZE 256

static char input_buf[INPUT_BUF_SIZE];
static int  input_pos = 0;
static volatile int cmd_ready = 0;

void kbd_set_callback(void (*cb)(char c));

static void kbd_handler(char c) {
    if (c == '\n') {
        vga_putchar('\n');
        input_buf[input_pos] = '\0';
        input_pos = 0;
        cmd_ready = 1;
    } else if (c == '\b' && input_pos > 0) {
        input_pos--;
        vga_putchar('\b');
    } else if (c >= 32 && c < 127 && input_pos < INPUT_BUF_SIZE - 1) {
        input_buf[input_pos++] = c;
        vga_putchar(c);
    }
}

static void prompt(void) {
    vga_set_color(LCYAN, BLACK);
    vga_write("basic> ");
    vga_set_color(LGRAY, BLACK);
}

static void cmd_help(void) {
    vga_write_color("\n  Basic Kernel v2.0 Commands:\n", YELLOW);
    vga_write("  help     - 显示此帮助\n");
    vga_write("  clear    - 清屏\n");
    vga_write("  version  - 内核版本\n");
    vga_write("  about    - 关于\n");
    vga_write("  ls       - 列出文件\n");
    vga_write("  cat      - 查看文件内容\n");
    vga_write("  free     - 内存使用\n");
    vga_write("  reboot   - 重启\n");
    vga_write("\n");
}

static void cmd_ls(void) {
    if (!mounted_fs) {
        vga_write("\n  No filesystem mounted\n");
        return;
    }
    fat32_file_t files[64];
    int n = fat32_readdir(mounted_fs, "/", files, 64);
    if (n < 0) {
        vga_write("\n  Error reading directory\n");
        return;
    }
    vga_write("\n");
    for (int i = 0; i < n; i++) {
        if (files[i].is_dir) {
            vga_write_color("  [DIR]  ", LGREEN);
        } else {
            vga_write("  [FILE] ");
        }
        vga_write(files[i].name);
        if (!files[i].is_dir) {
            vga_write("  (");
            vga_dec(files[i].size);
            vga_write(" bytes)");
        }
        vga_write("\n");
    }
    vga_write("  ");
    vga_dec(n);
    vga_write(" entries\n");
}

static void cmd_cat(const char *path) {
    if (!mounted_fs) {
        vga_write("\n  No filesystem mounted\n");
        return;
    }
    fat32_file_t *f = fat32_open(mounted_fs, path);
    if (!f) {
        vga_write("\n  File not found: ");
        vga_write(path);
        vga_write("\n");
        return;
    }
    vga_write("\n");
    char buf[512];
    int total = 0;
    while (1) {
        int n = fat32_read(f, buf, 512);
        if (n <= 0) break;
        for (int i = 0; i < n; i++) vga_putchar(buf[i]);
        total += n;
        if (total > 4096) { vga_write("\n... (truncated)\n"); break; }
    }
    vga_write("\n");
    fat32_close(f);
}

static void cmd_free(void) {
    vga_write("\n  Free memory: ");
    vga_dec(mm_get_free());
    vga_write(" MB\n");
}

static void cmd_version(void) {
    vga_write("\n  Basic Kernel v2.0.0\n");
    vga_write("  Built: " __DATE__ " " __TIME__ "\n");
    vga_write("  Arch:  i386 (32-bit x86)\n");
    vga_write("  Features: MMU | Paging | FAT32 | ATA\n\n");
}

static void cmd_about(void) {
    vga_write("\n");
    vga_write_color("  ============================================\n", GREEN);
    vga_write_color("   Basic Kernel v2.0 — 完整操作系统内核\n", GREEN);
    vga_write_color("   内存管理 | 分页 | FAT32 | ATA 磁盘\n", GREEN);
    vga_write_color("   Author: Basic OS Team\n", GREEN);
    vga_write_color("   GitHub: github.com/HuHuBasic\n", GREEN);
    vga_write_color("  ============================================\n", GREEN);
    vga_write("\n");
}

static void cmd_reboot(void) {
    vga_write("\n  Rebooting...\n");
    uint8 good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    for (;;) __asm__ volatile ("hlt");
}

void shell_exec(const char *cmd) {
    if (cmd[0] == '\0') return;
    if (strcmp(cmd, "help") == 0) cmd_help();
    else if (strcmp(cmd, "clear") == 0) vga_clear();
    else if (strcmp(cmd, "version") == 0) cmd_version();
    else if (strcmp(cmd, "about") == 0) cmd_about();
    else if (strcmp(cmd, "ls") == 0) cmd_ls();
    else if (strcmp(cmd, "free") == 0) cmd_free();
    else if (strcmp(cmd, "reboot") == 0) cmd_reboot();
    else if (strncmp(cmd, "cat ", 4) == 0) cmd_cat(cmd + 4);
    else {
        vga_write("\n  Unknown: ");
        vga_write(cmd);
        vga_write("\n  Type 'help' for available commands.\n");
    }
}

void shell_init(void) {
    kbd_set_callback(kbd_handler);
    vga_write("  [OK] Shell ready\n");
}

void shell_run(void) {
    prompt();
    for (;;) {
        if (cmd_ready) {
            cmd_ready = 0;
            shell_exec(input_buf);
            prompt();
        }
        __asm__ volatile ("hlt");
    }
}