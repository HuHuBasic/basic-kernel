/*
 * irq.h - IRQ 中断处理头文件
 */
#ifndef _IRQ_H
#define _IRQ_H

#include "types.h"

/* IRQ 处理函数 */
void irq_handler(registers_t *regs);

/* 初始化 IRQ */
void irq_init(void);

/* 注册 IRQ 处理函数 */
void irq_register_handler(uint8_t irq, isr_handler_t handler);

#endif /* _IRQ_H */