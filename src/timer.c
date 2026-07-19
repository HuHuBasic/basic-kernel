/*
 * timer.c - PIT (可编程间隔定时器) 实现
 * 使用 IRQ0, 频率 1.193182 MHz
 */
#include "timer.h"
#include "ports.h"
#include "irq.h"
#include "vga.h"
#include "types.h"

static volatile uint32_t timer_ticks = 0;
static uint32_t timer_frequency = 0;

static void timer_callback(registers_t *regs)
{
    (void)regs;
    timer_ticks++;
}

void timer_init(uint32_t frequency)
{
    timer_frequency = frequency;
    timer_ticks = 0;

    /* 注册 IRQ0 处理函数 */
    irq_register_handler(0, timer_callback);

    /* 计算分频值 */
    uint32_t divisor = 1193180 / frequency;

    /* 设置 PIT: 通道 0, 方波模式, 二进制计数 */
    outb(0x43, 0x36);

    /* 写分频值 (低字节, 高字节) */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t timer_get_ticks(void)
{
    return timer_ticks;
}

uint32_t timer_get_seconds(void)
{
    return timer_ticks / timer_frequency;
}

void timer_sleep(uint32_t ms)
{
    uint32_t start = timer_ticks;
    uint32_t target_ticks = ms * timer_frequency / 1000;
    if (target_ticks == 0) target_ticks = 1;

    while (timer_ticks - start < target_ticks) {
        __asm__ volatile("hlt");
    }
}