/*
 * vga.h - VGA 文本模式显示驱动头文件
 */
#ifndef _VGA_H
#define _VGA_H

#include <stdint.h>
#include <stddef.h>

/* VGA 颜色 */
enum vga_color {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,
    VGA_COLOR_WHITE         = 15,
};

/* 别名 */
#define VGA_COLOR_YELLOW VGA_COLOR_BROWN
#define VGA_COLOR_GREY   VGA_COLOR_LIGHT_GREY

/* 终端尺寸 */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* 初始化终端 */
void terminal_init(void);

/* 设置终端颜色 */
void terminal_set_color(uint8_t fg, uint8_t bg);

/* 写入字符 */
void terminal_putchar(char c);

/* 写入字符串 */
void terminal_write(const char *data, size_t size);

/* 写入以 null 结尾的字符串 */
void terminal_print(const char *str);

/* 清屏 */
void terminal_clear(void);

/* 获取光标位置 */
void terminal_get_cursor(int *row, int *col);

/* 设置光标位置 */
void terminal_set_cursor(int row, int col);

/* 滚动一行 */
void terminal_scroll(void);

/* 更新硬件光标 */
void terminal_update_cursor(void);

/* 打印十六进制数 */
void terminal_print_hex(uint32_t n);

/* 打印十进制数 */
void terminal_print_dec(uint32_t n);

#endif /* _VGA_H */