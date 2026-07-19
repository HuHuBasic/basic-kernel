/*
 * types.h - 内核通用类型定义
 */
#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>

/* 寄存器结构体 */
typedef struct {
    uint32_t ds;                                          /* 数据段选择子 */
    uint32_t edi, esi, ebp, useless, ebx, edx, ecx, eax; /* pusha 推送 */
    uint32_t int_no, err_code;                            /* 中断号和错误码 */
    uint32_t eip, cs, eflags, esp, ss;                    /* 由 CPU 自动推送 */
} __attribute__((packed)) registers_t;

/* 函数指针类型 */
typedef void (*isr_handler_t)(registers_t *);

/* IDT 条目 */
typedef struct {
    uint16_t base_low;   /* 处理函数地址低 16 位 */
    uint16_t selector;   /* 内核代码段选择子 */
    uint8_t  always0;    /* 始终为 0 */
    uint8_t  flags;      /* 标志位 */
    uint16_t base_high;  /* 处理函数地址高 16 位 */
} __attribute__((packed)) idt_entry_t;

/* IDT 指针 */
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

#endif /* _TYPES_H */