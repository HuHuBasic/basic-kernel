/*
 * idt.h - 中断描述符表头文件
 */
#ifndef _IDT_H
#define _IDT_H

#include <stdint.h>
#include "types.h"

/* 中断号 */
#define IRQ_BASE 0x20  /* IRQ 0-15 映射到 IDT 0x20-0x2F */

/* 初始化 IDT */
void idt_init(void);

/* 设置 IDT 条目 */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags);

/* 注册中断处理函数 */
void register_interrupt_handler(uint8_t n, isr_handler_t handler);

#endif /* _IDT_H */