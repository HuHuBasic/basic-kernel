/*
 * ports.h - I/O 端口操作头文件
 */
#ifndef _PORTS_H
#define _PORTS_H

#include <stdint.h>

/* 从端口读取一个字节 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/* 向端口写一个字节 */
static inline void outb(uint16_t port, uint8_t data)
{
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

/* 从端口读取一个字 (16-bit) */
static inline uint16_t inw(uint16_t port)
{
    uint16_t result;
    __asm__ volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/* 向端口写一个字 (16-bit) */
static inline void outw(uint16_t port, uint16_t data)
{
    __asm__ volatile("outw %0, %1" : : "a"(data), "Nd"(port));
}

/* 从端口读取一个双字 (32-bit) */
static inline uint32_t inl(uint16_t port)
{
    uint32_t result;
    __asm__ volatile("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/* 向端口写一个双字 (32-bit) */
static inline void outl(uint16_t port, uint32_t data)
{
    __asm__ volatile("outl %0, %1" : : "a"(data), "Nd"(port));
}

/* I/O 等待 (短暂延迟) */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

#endif /* _PORTS_H */