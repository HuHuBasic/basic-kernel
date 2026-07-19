/*
 * gdt.h - 全局描述符表头文件
 */
#ifndef _GDT_H
#define _GDT_H

#include <stdint.h>

/* GDT 选择子 */
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20

/* 初始化 GDT */
void gdt_init(void);

/* 刷新 GDT (汇编实现) */
extern void gdt_flush(uint32_t gdt_ptr);

#endif /* _GDT_H */