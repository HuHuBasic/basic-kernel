/*
 * isr.c - 中断服务例程实现
 * 处理 CPU 异常 (0-31)
 */
#include "isr.h"
#include "vga.h"
#include "idt.h"

/* 异常名称 */
const char *exception_names[] = {
    "Division Error",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

void isr_handler(registers_t *regs)
{
    terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal_print("\n[EXCEPTION] ");
    terminal_print(exception_names[regs->int_no]);
    terminal_print(" (int=");
    terminal_print_dec(regs->int_no);
    terminal_print(", err=");
    terminal_print_hex(regs->err_code);
    terminal_print(")\n");
    terminal_print("  EIP=");
    terminal_print_hex(regs->eip);
    terminal_print("\n");

    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    /* 对于严重异常, 挂起系统 */
    if (regs->int_no == 8 || regs->int_no == 13 || regs->int_no == 14) {
        terminal_print("  System halted!\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }
}

/* ---- ISR 汇编桩: 保存寄存器并调用 isr_handler ---- */

/* 无错误码的中断桩 */
#define ISR_NOERR(n)                                        \
    __asm__(".global isr" #n "\n"                           \
            "isr" #n ":\n"                                  \
            "    pushl $0\n"           /* 占位错误码 */       \
            "    pushl $" #n "\n"      /* 中断号 */           \
            "    jmp isr_common\n")

/* 有错误码的中断桩 */
#define ISR_ERR(n)                                          \
    __asm__(".global isr" #n "\n"                           \
            "isr" #n ":\n"                                  \
            "    pushl $" #n "\n"                           \
            "    jmp isr_common\n")

ISR_NOERR(0);
ISR_NOERR(1);
ISR_NOERR(2);
ISR_NOERR(3);
ISR_NOERR(4);
ISR_NOERR(5);
ISR_NOERR(6);
ISR_NOERR(7);
ISR_ERR(8);
ISR_NOERR(9);
ISR_ERR(10);
ISR_ERR(11);
ISR_ERR(12);
ISR_ERR(13);
ISR_ERR(14);
ISR_NOERR(15);
ISR_NOERR(16);
ISR_ERR(17);
ISR_NOERR(18);
ISR_NOERR(19);
ISR_NOERR(20);
ISR_NOERR(21);
ISR_NOERR(22);
ISR_NOERR(23);
ISR_NOERR(24);
ISR_NOERR(25);
ISR_NOERR(26);
ISR_NOERR(27);
ISR_NOERR(28);
ISR_NOERR(29);
ISR_ERR(30);
ISR_NOERR(31);

/* 通用 ISR 入口 */
__asm__(
    ".global isr_common\n"
    "isr_common:\n"
    "    pusha\n"                    /* 保存所有通用寄存器 */
    "    movw %ds, %ax\n"
    "    pushl %eax\n"               /* 保存 DS */
    "    movw $0x10, %ax\n"          /* 加载内核数据段 */
    "    movw %ax, %ds\n"
    "    movw %ax, %es\n"
    "    movw %ax, %fs\n"
    "    movw %ax, %gs\n"
    "    movl %esp, %eax\n"          /* 传递寄存器结构体指针 */
    "    pushl %eax\n"
    "    call isr_handler\n"
    "    popl %eax\n"
    "    popl %eax\n"                /* 恢复 DS */
    "    movw %ax, %ds\n"
    "    movw %ax, %es\n"
    "    movw %ax, %fs\n"
    "    movw %ax, %gs\n"
    "    popa\n"                     /* 恢复通用寄存器 */
    "    addl $8, %esp\n"            /* 清除错误码和中断号 */
    "    sti\n"                      /* 开中断 */
    "    iret\n"                     /* 中断返回 */
);