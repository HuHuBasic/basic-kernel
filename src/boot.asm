; boot.asm — Basic Kernel 启动代码
; Multiboot 1 规范，32位保护模式

MBALIGN    equ 1<<0
MEMINFO    equ 1<<1
MBFLAGS    equ MBALIGN | MEMINFO
MAGIC      equ 0x1BADB002
CHECKSUM   equ -(MAGIC + MBFLAGS)

section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384          ; 16KB 内核栈
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top  ; 设置栈指针
    push eax            ; 保存 Multiboot magic
    push ebx            ; 保存 Multiboot info 指针
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang