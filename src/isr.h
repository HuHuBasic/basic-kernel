/*
 * isr.h - 中断服务例程头文件
 */
#ifndef _ISR_H
#define _ISR_H

#include "types.h"

/* 异常名称 */
extern const char *exception_names[];

/* ISR 处理函数 */
void isr_handler(registers_t *regs);

#endif /* _ISR_H */