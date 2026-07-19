/*
 * gdt.c - 全局描述符表初始化
 */
#include "gdt.h"

/* GDT 指针 (在 boot.s 中定义) */
extern uint8_t gdt_descriptor;

void gdt_init(void)
{
    gdt_flush((uint32_t)&gdt_descriptor);
}