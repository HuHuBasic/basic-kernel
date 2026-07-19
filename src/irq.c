/*
 * irq.c - IRQ 中断处理实现
 * 处理硬件中断 (IRQ 0-15)
 */
#include "irq.h"
#include "ports.h"
#include "vga.h"
#include "idt.h"

/* 外部 IRQ 处理函数注册表 */
static isr_handler_t irq_handlers[16];

/* ---- IRQ 汇编桩 ---- */

#define IRQ_STUB(n)                                          \
    __asm__(".global irq" #n "\n"                            \
            "irq" #n ":\n"                                   \
            "    pushl $0\n"                                 \
            "    pushl $" #n "\n"                            \
            "    jmp irq_common\n")

IRQ_STUB(0);
IRQ_STUB(1);
IRQ_STUB(2);
IRQ_STUB(3);
IRQ_STUB(4);
IRQ_STUB(5);
IRQ_STUB(6);
IRQ_STUB(7);
IRQ_STUB(8);
IRQ_STUB(9);
IRQ_STUB(10);
IRQ_STUB(11);
IRQ_STUB(12);
IRQ_STUB(13);
IRQ_STUB(14);
IRQ_STUB(15);

/* 通用 IRQ 入口 */
__asm__(
    ".global irq_common\n"
    "irq_common:\n"
    "    pusha\n"
    "    movw %ds, %ax\n"
    "    pushl %eax\n"
    "    movw $0x10, %ax\n"
    "    movw %ax, %ds\n"
    "    movw %ax, %es\n"
    "    movw %ax, %fs\n"
    "    movw %ax, %gs\n"
    "    movl %esp, %eax\n"
    "    pushl %eax\n"
    "    call irq_handler\n"
    "    popl %eax\n"
    "    popl %eax\n"
    "    movw %ax, %ds\n"
    "    movw %ax, %es\n"
    "    movw %ax, %fs\n"
    "    movw %ax, %gs\n"
    "    popa\n"
    "    addl $8, %esp\n"
    "    sti\n"
    "    iret\n"
);

void irq_handler(registers_t *regs)
{
    /* 发送 EOI (End of Interrupt) */
    if (regs->int_no >= 40) {
        outb(0xA0, 0x20);  /* 从 PIC */
    }
    outb(0x20, 0x20);  /* 主 PIC */

    /* 调用注册的处理函数 */
    uint8_t irq = regs->int_no - 32;
    if (irq_handlers[irq] != 0) {
        irq_handlers[irq](regs);
    }
}

void irq_register_handler(uint8_t irq, isr_handler_t handler)
{
    irq_handlers[irq] = handler;
}

void irq_init(void)
{
    /* 注册 IRQ 处理函数 */
    for (int i = 0; i < 16; i++) {
        irq_handlers[i] = 0;
    }
}