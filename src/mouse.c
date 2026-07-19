/*
 * mouse.c - PS/2 鼠标驱动实现
 * 
 * PS/2 鼠标协议:
 *   - IRQ12 中断
 *   - 3 字节数据包: [flags+buttons][X_movement][Y_movement]
 *   - 端口 0x60 数据, 0x64 状态/命令
 * 
 * 光标在 VGA 文本模式下渲染, 通过反转字符颜色实现
 */
#include "mouse.h"
#include "ports.h"
#include "irq.h"
#include "vga.h"
#include "string.h"

/* VGA 缓冲区 */
#define VGA_BUF ((uint16_t *)0xB8000)

/* 鼠标状态 */
static volatile int mouse_x       = 40;   /* 当前 X (字符列) */
static volatile int mouse_y       = 12;   /* 当前 Y (字符行) */
static volatile int mouse_buttons = 0;    /* 当前按键状态 */
static volatile int mouse_clicked = 0;    /* 点击事件: bit0=左键, bit1=右键 */
static volatile int mouse_moved_flag = 0; /* 移动标志 */

/* 用于点击检测 */
static volatile int prev_buttons = 0;

/* 光标保存 */
static volatile int  cursor_visible = 0;
static volatile int  cursor_saved_x = 0;
static volatile int  cursor_saved_y = 0;
static uint16_t      cursor_saved_char = 0;

/* 点击反馈 */
static volatile int   tap_feedback = 0;
static volatile int   tap_fb_x = 0;
static volatile int   tap_fb_y = 0;

/* 绝对模式 (触摸屏/平板) */
static volatile int   absolute_mode = 0;  /* 0=相对移动, 1=绝对定位 */
static volatile int   abs_x = 0;          /* 绝对 X 原始值 */
static volatile int   abs_y = 0;          /* 绝对 Y 原始值 */

/* 中断数据包解析状态 */
static volatile int   mouse_cycle = 0;
static volatile int   mouse_packet_size = 3;  /* 3 字节(相对) 或 5 字节(绝对) */
static volatile uint8_t mouse_bytes[5];

/* ---- PS/2 鼠标辅助函数 ---- */

static inline void mouse_wait(uint8_t type)
{
    int timeout = 100000;
    if (type == 0) {
        /* 等待数据可读 */
        while (timeout--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        /* 等待写就绪 */
        while (timeout--) {
            if ((inb(0x64) & 2) == 0) return;
        }
    }
}

static inline void mouse_write(uint8_t val)
{
    mouse_wait(1);
    outb(0x64, 0xD4);   /* 告知控制器下一字节发给鼠标 */
    mouse_wait(1);
    outb(0x60, val);
}

static inline uint8_t mouse_read(void)
{
    mouse_wait(0);
    return inb(0x60);
}

/* ---- 鼠标 IRQ12 中断处理 ---- */

static void mouse_irq_handler(registers_t *regs)
{
    (void)regs;
    uint8_t status = inb(0x64);

    if (!(status & 1) || !(status & 0x20)) return;

    uint8_t data = inb(0x60);

    if (absolute_mode) {
        /* 绝对模式: 5 字节数据包 */
        /* 字节 0: flags+buttons, 字节 1-2: X, 字节 3-4: Y */
        mouse_bytes[mouse_cycle] = data;
        mouse_cycle++;

        if (mouse_cycle >= 4) {
            mouse_cycle = 0;

            /* 解析按键 */
            int new_buttons = 0;
            if (mouse_bytes[0] & 0x01) new_buttons |= MOUSE_LEFT;
            if (mouse_bytes[0] & 0x02) new_buttons |= MOUSE_RIGHT;
            if (mouse_bytes[0] & 0x04) new_buttons |= MOUSE_MIDDLE;

            int click = 0;
            if ((prev_buttons & MOUSE_LEFT) && !(new_buttons & MOUSE_LEFT))
                click |= MOUSE_LEFT;
            if ((prev_buttons & MOUSE_RIGHT) && !(new_buttons & MOUSE_RIGHT))
                click |= MOUSE_RIGHT;
            mouse_clicked = click;
            prev_buttons = new_buttons;
            mouse_buttons = new_buttons;

            /* 绝对坐标: 字节1=X低, 字节2=X高, 字节3=Y低, 字节4=Y高 */
            int raw_x = (int)mouse_bytes[1] | ((int)mouse_bytes[2] << 8);
            int raw_y = (int)mouse_bytes[3] | ((int)mouse_bytes[4] << 8);

            /* 缩放到 80x25 字符网格 */
            int nx = (raw_x * VGA_WIDTH) / 32768;
            int ny = (raw_y * VGA_HEIGHT) / 32768;

            if (nx < 0) nx = 0;
            if (nx >= VGA_WIDTH) nx = VGA_WIDTH - 1;
            if (ny < 0) ny = 0;
            if (ny >= VGA_HEIGHT) ny = VGA_HEIGHT - 1;

            if (nx != mouse_x || ny != mouse_y) {
                mouse_x = nx;
                mouse_y = ny;
                mouse_moved_flag = 1;
            }
        }
        return;
    }

    /* 相对模式: 3 字节数据包 */
    switch (mouse_cycle) {
        case 0:
            if (!(data & 0x08)) break;
            mouse_bytes[0] = data;
            mouse_cycle = 1;
            break;
        case 1:
            mouse_bytes[1] = data;
            mouse_cycle = 2;
            break;
        case 2:
            mouse_bytes[2] = data;
            mouse_cycle = 0;

            /* 解析按键 */
            int new_buttons = 0;
            if (mouse_bytes[0] & 0x01) new_buttons |= MOUSE_LEFT;
            if (mouse_bytes[0] & 0x02) new_buttons |= MOUSE_RIGHT;
            if (mouse_bytes[0] & 0x04) new_buttons |= MOUSE_MIDDLE;

            int click = 0;
            if ((prev_buttons & MOUSE_LEFT) && !(new_buttons & MOUSE_LEFT))
                click |= MOUSE_LEFT;
            if ((prev_buttons & MOUSE_RIGHT) && !(new_buttons & MOUSE_RIGHT))
                click |= MOUSE_RIGHT;
            mouse_clicked = click;
            prev_buttons = new_buttons;
            mouse_buttons = new_buttons;

            int x_move = (int)mouse_bytes[1];
            int y_move = (int)mouse_bytes[2];

            if (mouse_bytes[0] & 0x10) x_move = (int)(mouse_bytes[1]) - 256;
            if (mouse_bytes[0] & 0x20) y_move = (int)(mouse_bytes[2]) - 256;

            y_move = -y_move;

            int nx = mouse_x + (x_move / 2);
            int ny = mouse_y + (y_move / 2);

            if (nx < 0) nx = 0;
            if (nx >= VGA_WIDTH) nx = VGA_WIDTH - 1;
            if (ny < 0) ny = 0;
            if (ny >= VGA_HEIGHT) ny = VGA_HEIGHT - 1;

            if (nx != mouse_x || ny != mouse_y) {
                mouse_x = nx;
                mouse_y = ny;
                mouse_moved_flag = 1;
            }
            break;
    }
}

/* ---- 初始化 ---- */

void mouse_init(void)
{
    mouse_x = 40;
    mouse_y = 12;
    mouse_buttons = 0;
    mouse_clicked = 0;
    mouse_cycle = 0;
    prev_buttons = 0;
    cursor_visible = 0;
    mouse_moved_flag = 0;
    absolute_mode = 0;
    mouse_packet_size = 3;

    /* 注册 IRQ12 中断处理 */
    irq_register_handler(12, mouse_irq_handler);

    /* 启用鼠标 */
    mouse_wait(1);
    outb(0x64, 0xA8);  /* 启用辅助设备 (鼠标) */

    /* 启用鼠标中断 */
    mouse_wait(1);
    outb(0x64, 0x20);  /* 读取配置字节 */
    mouse_wait(0);
    uint8_t config = inb(0x60);
    config |= 0x02;     /* 启用鼠标 IRQ12 */
    config &= ~0x20;    /* 清除鼠标时钟禁用位 */
    mouse_wait(1);
    outb(0x64, 0x60);  /* 写回配置字节 */
    mouse_wait(1);
    outb(0x60, config);

    /* 重置鼠标 */
    mouse_write(0xFF);
    mouse_read();  /* ACK */
    mouse_read();  /* AA (self-test OK) */
    mouse_read();  /* 00 (device ID) */

    /* 尝试启用绝对模式 (触摸屏/平板握手协议) */
    /* 发送 Set Sample Rate 序列: 200 → 100 → 80 */
    mouse_write(0xF3); mouse_read();  /* Set Sample Rate */
    mouse_write(200);  mouse_read();  /* 200 */
    mouse_write(0xF3); mouse_read();
    mouse_write(100);  mouse_read();  /* 100 */
    mouse_write(0xF3); mouse_read();
    mouse_write(80);   mouse_read();  /* 80 */

    /* 读取设备 ID 检测是否进入绝对模式 */
    mouse_write(0xF2);               /* Get Device ID */
    mouse_read();                    /* ACK */
    int dev_id = mouse_read();       /* 0=标准鼠标, 3=带滚轮, 4=5键 */

    if (dev_id == 3 || dev_id == 4) {
        /* 滚轮/多键鼠标 — 可能支持绝对模式 */
        /* 部分触摸屏设备在这里返回不同的 ID */
        absolute_mode = 0;  /* 默认保持相对模式, 最稳定 */
    }

    /* 启用鼠标数据报告 */
    mouse_write(0xF4);
    mouse_read();  /* ACK */
}

/* 设置绝对位置 (供触摸屏等外部输入设备调用) */
void mouse_set_position(int x, int y)
{
    if (x < 0) x = 0;
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;

    if (x != mouse_x || y != mouse_y) {
        mouse_x = x;
        mouse_y = y;
        mouse_moved_flag = 1;
    }
}

/* ---- 公共接口 ---- */

int mouse_get_x(void)     { return mouse_x; }
int mouse_get_y(void)     { return mouse_y; }
int mouse_get_buttons(void) { return mouse_buttons; }
int mouse_left_down(void) { return (mouse_buttons & MOUSE_LEFT) != 0; }

int mouse_left_click(void)
{
    int ret = (mouse_clicked & MOUSE_LEFT) != 0;
    mouse_clicked &= ~MOUSE_LEFT;
    return ret;
}

int mouse_right_click(void)
{
    int ret = (mouse_clicked & MOUSE_RIGHT) != 0;
    mouse_clicked &= ~MOUSE_RIGHT;
    return ret;
}

int mouse_moved(void)
{
    int ret = mouse_moved_flag;
    mouse_moved_flag = 0;
    return ret;
}

/* ---- 光标渲染 ---- */

void mouse_hide(void)
{
    if (!cursor_visible) return;
    /* 恢复被光标覆盖的原始字符 */
    VGA_BUF[cursor_saved_y * VGA_WIDTH + cursor_saved_x] = cursor_saved_char;
    cursor_visible = 0;
}

void mouse_draw(void)
{
    if (cursor_visible) {
        if (cursor_saved_x != mouse_x || cursor_saved_y != mouse_y) {
            VGA_BUF[cursor_saved_y * VGA_WIDTH + cursor_saved_x] = cursor_saved_char;
        } else {
            return;
        }
    }

    /* 保存新位置原始字符 */
    cursor_saved_x = mouse_x;
    cursor_saved_y = mouse_y;
    cursor_saved_char = VGA_BUF[mouse_y * VGA_WIDTH + mouse_x];

    /* 点击反馈闪烁 */
    if (tap_feedback) {
        /* 用高亮白色圆点显示点击位置 */
        VGA_BUF[mouse_y * VGA_WIDTH + mouse_x] = (uint16_t)0x04 | (uint16_t)(0x0F) << 8;
        tap_feedback = 0;  /* 一帧后清除 */
        cursor_visible = 1;
        return;
    }

    /* 正常光标: 白色箭头 ▸ (0x10)  */
    uint8_t ch = cursor_saved_char & 0xFF;
    /* 如果原字符是空格, 用实心方块; 否则用箭头叠加 */
    if (ch == ' ' || ch == 0) {
        VGA_BUF[mouse_y * VGA_WIDTH + mouse_x] = (uint16_t)0xDB | (uint16_t)(0x0F) << 8;
    } else {
        /* 白色箭头高亮 */
        VGA_BUF[mouse_y * VGA_WIDTH + mouse_x] = (uint16_t)0x10 | (uint16_t)(0x0F) << 8;
    }

    cursor_visible = 1;
}

/* 触发点击视觉反馈 */
void mouse_tap_feedback(void)
{
    tap_feedback = 1;
    tap_fb_x = mouse_x;
    tap_fb_y = mouse_y;
}