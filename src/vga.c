/* vga.c — VGA 文本模式驱动 */

#include "kernel.h"

static uint16 *const vga_buffer = (uint16*)VGA_ADDR;
static int vga_row = 0, vga_col = 0;
static uint8 vga_color = 0x0F;

uint8 make_color(uint8 fg, uint8 bg) { return fg | (bg << 4); }
static inline uint16 make_entry(char c, uint8 color) { return (uint16)c | ((uint16)color << 8); }

void vga_set_color(uint8 fg, uint8 bg) { vga_color = make_color(fg, bg); }

void vga_clear(void) {
    uint16 blank = make_entry(' ', vga_color);
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga_buffer[i] = blank;
    vga_row = vga_col = 0;
}

static void vga_scroll(void) {
    for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++)
        vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
    uint16 blank = make_entry(' ', vga_color);
    for (int i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga_buffer[i] = blank;
    vga_row = VGA_HEIGHT - 1;
    vga_col = 0;
}

void vga_putchar(char c) {
    if (c == '\n') { vga_col = 0; vga_row++; }
    else if (c == '\b') { if (vga_col > 0) vga_col--; vga_buffer[vga_row * VGA_WIDTH + vga_col] = make_entry(' ', vga_color); }
    else if (c == '\r') { vga_col = 0; }
    else if (c == '\t') { vga_col = (vga_col + 8) & ~7; }
    else { vga_buffer[vga_row * VGA_WIDTH + vga_col] = make_entry(c, vga_color); vga_col++; }
    if (vga_col >= VGA_WIDTH) { vga_col = 0; vga_row++; }
    if (vga_row >= VGA_HEIGHT) vga_scroll();
}

void vga_write(const char *s) { while (*s) vga_putchar(*s++); }

void vga_write_color(const char *s, uint8 fg) {
    uint8 old = vga_color;
    vga_color = make_color(fg, BLACK);
    vga_write(s);
    vga_color = old;
}

void vga_hex(uint32 n) {
    char buf[11];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 9; i >= 2; i--) { int d = n & 0xF; buf[i] = d < 10 ? '0' + d : 'A' + d - 10; n >>= 4; }
    buf[10] = '\0';
    vga_write(buf);
}

void vga_dec(uint32 n) {
    if (n == 0) { vga_putchar('0'); return; }
    char buf[11]; int i = 10;
    buf[10] = '\0';
    while (n > 0) { buf[--i] = '0' + (n % 10); n /= 10; }
    vga_write(&buf[i]);
}