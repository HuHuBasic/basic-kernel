/*
 * vga.c - VGA 文本模式显示驱动
 * 支持 80x25 文本模式, 16 种颜色
 */
#include "vga.h"
#include "ports.h"
#include "string.h"

/* VGA 帧缓冲地址 */
#define VGA_MEMORY  ((uint16_t *)0xB8000)

/* VGA I/O 端口 */
#define VGA_CTRL_PORT  0x3D4
#define VGA_DATA_PORT  0x3D5

/* 内部状态 */
static int   terminal_row;
static int   terminal_column;
static uint8_t terminal_color;
static uint16_t *terminal_buffer;

/* 组成 VGA 条目: 字符 + 颜色属性 */
static inline uint16_t vga_entry(unsigned char c, uint8_t color)
{
    return (uint16_t)c | (uint16_t)color << 8;
}

/* 组成颜色值: 前景色 + 背景色 << 4 */
static inline uint8_t vga_color(enum vga_color fg, enum vga_color bg)
{
    return fg | bg << 4;
}

void terminal_init(void)
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_buffer = VGA_MEMORY;

    terminal_clear();
}

void terminal_set_color(uint8_t fg, uint8_t bg)
{
    terminal_color = vga_color(fg, bg);
}

void terminal_clear(void)
{
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
        }
    }
    terminal_row = 0;
    terminal_column = 0;
    terminal_update_cursor();
}

void terminal_scroll(void)
{
    /* 向上滚动一行 */
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = terminal_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    /* 清空最后一行 */
    for (int x = 0; x < VGA_WIDTH; x++) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
}

void terminal_putchar(char c)
{
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row >= VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
    } else if (c == '\r') {
        terminal_column = 0;
    } else if (c == '\t') {
        /* 制表符: 4 空格对齐 */
        terminal_column = (terminal_column + 4) & ~3;
        if (terminal_column >= VGA_WIDTH) {
            terminal_column = 0;
            if (++terminal_row >= VGA_HEIGHT) {
                terminal_scroll();
                terminal_row = VGA_HEIGHT - 1;
            }
        }
    } else if (c == '\b') {
        /* 退格 */
        if (terminal_column > 0) {
            terminal_column--;
            terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = vga_entry(' ', terminal_color);
        }
    } else {
        terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = vga_entry(c, terminal_color);
        if (++terminal_column >= VGA_WIDTH) {
            terminal_column = 0;
            if (++terminal_row >= VGA_HEIGHT) {
                terminal_scroll();
                terminal_row = VGA_HEIGHT - 1;
            }
        }
    }
    terminal_update_cursor();
}

void terminal_write(const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        terminal_putchar(data[i]);
    }
}

void terminal_print(const char *str)
{
    terminal_write(str, strlen(str));
}

void terminal_get_cursor(int *row, int *col)
{
    *row = terminal_row;
    *col = terminal_column;
}

void terminal_set_cursor(int row, int col)
{
    terminal_row = row;
    terminal_column = col;
    terminal_update_cursor();
}

void terminal_update_cursor(void)
{
    uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;

    /* 写光标位置高字节 */
    outb(VGA_CTRL_PORT, 14);
    outb(VGA_DATA_PORT, (pos >> 8) & 0xFF);

    /* 写光标位置低字节 */
    outb(VGA_CTRL_PORT, 15);
    outb(VGA_DATA_PORT, pos & 0xFF);
}

void terminal_print_hex(uint32_t n)
{
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    int pos = 2;

    for (int i = 28; i >= 0; i -= 4) {
        int nibble = (n >> i) & 0xF;
        if (nibble < 10)
            buf[pos++] = '0' + nibble;
        else
            buf[pos++] = 'A' + nibble - 10;
    }
    buf[pos] = '\0';
    terminal_print(buf);
}

void terminal_print_dec(uint32_t n)
{
    if (n == 0) {
        terminal_putchar('0');
        return;
    }

    char buf[12];
    int pos = 0;

    while (n > 0) {
        buf[pos++] = '0' + (n % 10);
        n /= 10;
    }

    /* 反转输出 */
    for (int i = pos - 1; i >= 0; i--) {
        terminal_putchar(buf[i]);
    }
}