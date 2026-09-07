/* io.c — 端口 I/O 工具 */

#include "kernel.h"

void outb(uint16 port, uint8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8 inb(uint16 port) {
    uint8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outw(uint16 port, uint16 val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

uint16 inw(uint16 port) {
    uint16 ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void io_wait(void) { outb(0x80, 0); }