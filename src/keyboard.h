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