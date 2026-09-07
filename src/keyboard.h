/*
 * keyboard.h - PS/2 键盘驱动头文件
 */
#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include <stdint.h>

/* 特殊键码 */
#define KEY_NONE    0
#define KEY_UP      0x100
#define KEY_DOWN    0x101
#define KEY_LEFT    0x102
#define KEY_RIGHT   0x103
#define KEY_ENTER   0x104
#define KEY_ESC     0x105
#define KEY_TAB     0x106
#define KEY_F1      0x110
#define KEY_F2      0x111
#define KEY_F3      0x112
#define KEY_F4      0x113
#define KEY_F5      0x114
#define KEY_F6      0x115
#define KEY_F7      0x116
#define KEY_F8      0x117
#define KEY_F9      0x118
#define KEY_F10     0x119
#define KEY_F11     0x11A
#define KEY_F12     0x11B

/* 初始化键盘 */
void keyboard_init(void);

/* 获取最近一次按键 (非阻塞, 无按键返回 0) */
char keyboard_getchar(void);

/* 非阻塞获取按键 */
char keyboard_getchar_nonblock(void);

/* 获取特殊键 (非阻塞, 无按键返回 0) */
int  keyboard_poll(void);

/* 清除特殊键缓冲区 */
void keyboard_clear_special(void);

/* 获取一行输入 (阻塞, 直到回车) */
void keyboard_readline(char *buffer, int max_len);

#endif /* _KEYBOARD_H */