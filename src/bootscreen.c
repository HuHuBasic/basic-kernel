/*
 * bootscreen.c - 开机动画实现
 * 在 VGA 文本模式下显示 "Basic" 彩色跳跃字母动画
 * 颜色: B=红, a=黄, s=蓝, i=绿, c=青
 * 纯整数运算，无浮点依赖
 */
#include "bootscreen.h"
#include "vga.h"
#include "timer.h"

/* VGA 帧缓冲 */
#define VGA_BUF  ((uint16_t *)0xB8000)

static inline void vga_put(int x, int y, char c, uint8_t color)
{
    VGA_BUF[y * VGA_WIDTH + x] = (uint16_t)c | (uint16_t)color << 8;
}

static uint8_t make_color(uint8_t fg, uint8_t bg)
{
    return fg | bg << 4;
}

/* 字母位图: 5 列 x 7 行 */
#define LETTER_W  5
#define LETTER_H  7
#define BLOCK 0xDB

static const uint8_t letter_B[7][5] = {
    {1,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,1},
    {1,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,1}, {1,1,1,1,0},
};

static const uint8_t letter_a[7][5] = {
    {0,1,1,1,0}, {1,0,0,0,1}, {0,0,0,0,1},
    {0,1,1,1,1}, {1,0,0,0,1}, {1,0,0,1,1}, {0,1,1,0,1},
};

static const uint8_t letter_s[7][5] = {
    {0,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,0},
    {0,1,1,1,0}, {0,0,0,0,1}, {1,0,0,0,1}, {0,1,1,1,0},
};

static const uint8_t letter_i[7][5] = {
    {0,0,1,0,0}, {0,0,0,0,0}, {0,0,1,0,0},
    {0,0,1,0,0}, {0,0,1,0,0}, {0,0,1,0,0}, {0,1,1,1,0},
};

static const uint8_t letter_c[7][5] = {
    {0,1,1,1,0}, {1,0,0,0,1}, {1,0,0,0,0},
    {1,0,0,0,0}, {1,0,0,0,0}, {1,0,0,0,1}, {0,1,1,1,0},
};

static const uint8_t letter_colors[5] = {
    VGA_COLOR_RED, VGA_COLOR_BROWN, VGA_COLOR_BLUE,
    VGA_COLOR_GREEN, VGA_COLOR_CYAN,
};

static const uint8_t *letter_bitmaps[5] = {
    (const uint8_t *)letter_B,
    (const uint8_t *)letter_a,
    (const uint8_t *)letter_s,
    (const uint8_t *)letter_i,
    (const uint8_t *)letter_c,
};

/* 绘制一个 5x7 大字母 */
static void draw_letter(int base_x, int base_y,
                        const uint8_t *bitmap, uint8_t fg_color)
{
    uint8_t clr = make_color(fg_color, VGA_COLOR_BLACK);
    for (int row = 0; row < LETTER_H; row++) {
        for (int col = 0; col < LETTER_W; col++) {
            vga_put(base_x + col, base_y + row,
                    bitmap[row * LETTER_W + col] ? BLOCK : ' ', clr);
        }
    }
}

/* 清除字母区域 (含跳跃空间) */
static void clear_letter_area(int base_x, int base_y)
{
    uint8_t clr = make_color(VGA_COLOR_BLACK, VGA_COLOR_BLACK);
    for (int row = 0; row < LETTER_H + 5; row++) {
        for (int col = 0; col < LETTER_W; col++) {
            vga_put(base_x + col, base_y + row, ' ', clr);
        }
    }
}

/* 绘制进度条 */
static void draw_progress(int filled, int total)
{
    int bar_x = 15, bar_y = 18, bar_w = 50;
    uint8_t border_clr = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    /* 边框 */
    vga_put(bar_x - 1, bar_y - 1, 0xDA, border_clr);
    vga_put(bar_x + bar_w, bar_y - 1, 0xBF, border_clr);
    vga_put(bar_x - 1, bar_y + 1, 0xC0, border_clr);
    vga_put(bar_x + bar_w, bar_y + 1, 0xD9, border_clr);
    for (int i = 0; i < bar_w; i++) {
        vga_put(bar_x + i, bar_y - 1, 0xC4, border_clr);
        vga_put(bar_x + i, bar_y + 1, 0xC4, border_clr);
    }
    vga_put(bar_x - 1, bar_y, 0xB3, border_clr);
    vga_put(bar_x + bar_w, bar_y, 0xB3, border_clr);

    int n = (filled * bar_w) / total;
    if (n > bar_w) n = bar_w;

    /* 渐变填充: 红→黄→蓝→绿→青 */
    for (int i = 0; i < n; i++) {
        uint8_t c;
        int seg = (i * 5) / bar_w;
        if (seg == 0)      c = VGA_COLOR_RED;
        else if (seg == 1) c = VGA_COLOR_BROWN;
        else if (seg == 2) c = VGA_COLOR_BLUE;
        else if (seg == 3) c = VGA_COLOR_GREEN;
        else               c = VGA_COLOR_CYAN;
        vga_put(bar_x + i, bar_y, BLOCK, make_color(c, VGA_COLOR_BLACK));
    }

    /* 清空剩余部分 */
    uint8_t empty = make_color(VGA_COLOR_BLACK, VGA_COLOR_BLACK);
    for (int i = n; i < bar_w; i++) {
        vga_put(bar_x + i, bar_y, ' ', empty);
    }
}

/* 绘制静态文字 */
static void draw_static_text(void)
{
    uint8_t dim = make_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    uint8_t mid = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    /* 标题 */
    static const char title[] = "H U   B A S I C   O S";
    int tx = (80 - (int)sizeof(title) + 1) / 2;  /* 居中 */
    for (int i = 0; title[i]; i++) {
        vga_put(tx + i, 14, title[i], mid);
    }

    /* 状态文字 */
    static const char status[] = "Loading...";
    int sx = (80 - (int)sizeof(status) + 1) / 2;
    for (int i = 0; status[i]; i++) {
        vga_put(sx + i, 20, status[i], dim);
    }

    /* 版本信息 */
    static const char ver[] = "64-bit  -  32-bit  -  Compatible";
    int vx = (80 - (int)sizeof(ver) + 1) / 2;
    for (int i = 0; ver[i]; i++) {
        vga_put(vx + i, 23, ver[i], dim);
    }
}

/* 开机动画主函数 */
void bootscreen_show(void)
{
    terminal_clear();

    /* 字母 X 基准位置: 5个字母各5宽+2间距=33, 屏幕80居中偏左(23) */
    int letter_x[5];
    int base_y = 3;

    letter_x[0] = 23;
    letter_x[1] = letter_x[0] + LETTER_W + 2;
    letter_x[2] = letter_x[1] + LETTER_W + 2;
    letter_x[3] = letter_x[2] + LETTER_W + 2;
    letter_x[4] = letter_x[3] + LETTER_W + 2;

    draw_static_text();

    /* 跳跃偏移表: 6帧一个周期, 0=原位, 4=最高, 2=半高 */
    static const int jump_table[6] = {0, 4, 0, 2, 0, 0};

    int total_frames = 24;
    for (int frame = 0; frame < total_frames; frame++) {
        /* 清除所有字母区域 */
        for (int L = 0; L < 5; L++) {
            clear_letter_area(letter_x[L], base_y);
        }

        /* 重新绘制 */
        for (int L = 0; L < 5; L++) {
            int adj = frame - L;
            if (adj < 0) adj = 0;
            int offset = jump_table[adj % 6];
            draw_letter(letter_x[L], base_y + offset,
                        letter_bitmaps[L], letter_colors[L]);
        }

        /* 进度条 */
        draw_progress(frame + 1, total_frames);

        /* 帧延迟 ~167ms */
        timer_sleep(167);
    }

    /* 短暂停留 */
    timer_sleep(600);

    /* 清屏 */
    terminal_clear();
    terminal_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
}