/*
 * mouse.h - PS/2 鼠标驱动头文件
 * 支持按键检测、坐标追踪和点击事件
 */
#ifndef _MOUSE_H
#define _MOUSE_H

#include <stdint.h>

/* 鼠标按键 */
#define MOUSE_LEFT    1
#define MOUSE_RIGHT   2
#define MOUSE_MIDDLE  4

/* 初始化鼠标 */
void mouse_init(void);

/* 获取鼠标 X 坐标 (字符列, 0-79) */
int  mouse_get_x(void);

/* 获取鼠标 Y 坐标 (字符行, 0-24) */
int  mouse_get_y(void);

/* 设置绝对位置 (触摸屏直接定位) */
void mouse_set_position(int x, int y);

/* 获取当前按键状态 */
int  mouse_get_buttons(void);

/* 检测左键点击 (边缘触发, 读后清零) */
int  mouse_left_click(void);

/* 检测右键点击 (边缘触发, 读后清零) */
int  mouse_right_click(void);

/* 检测左键是否按下 (电平触发) */
int  mouse_left_down(void);

/* 绘制鼠标光标 */
void mouse_draw(void);

/* 隐藏鼠标光标 (恢复被覆盖的字符) */
void mouse_hide(void);

/* 鼠标是否已移动 */
int  mouse_moved(void);

/* 触发点击视觉反馈 */
void mouse_tap_feedback(void);

#endif /* _MOUSE_H */