/*
 * timer.h - PIT (可编程间隔定时器) 头文件
 */
#ifndef _TIMER_H
#define _TIMER_H

#include <stdint.h>

/* 初始化定时器 */
void timer_init(uint32_t frequency);

/* 获取系统时钟滴答数 */
uint32_t timer_get_ticks(void);

/* 获取系统运行秒数 */
uint32_t timer_get_seconds(void);

/* 忙等待 (毫秒) */
void timer_sleep(uint32_t ms);

#endif /* _TIMER_H */