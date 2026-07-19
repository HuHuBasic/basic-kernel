/*
 * rtl8139.h - RTL8139 网卡驱动头文件
 */
#ifndef _RTL8139_H
#define _RTL8139_H

#include <stdint.h>

/* RTL8139 寄存器 */
#define RTL_IDR0     0x00
#define RTL_IDR1     0x01
#define RTL_IDR2     0x02
#define RTL_IDR3     0x03
#define RTL_IDR4     0x04
#define RTL_IDR5     0x05
#define RTL_RBSTART  0x30
#define RTL_CMD      0x37
#define RTL_CAPR     0x38
#define RTL_IMR      0x3C
#define RTL_ISR      0x3E
#define RTL_TCR      0x40
#define RTL_RCR      0x44
#define RTL_CONFIG1  0x52

/* 发送描述符 */
#define RTL_TSAD0    0x20
#define RTL_TSD0     0x10

/* 命令位 */
#define CMD_RX_ENABLE  0x08
#define CMD_TX_ENABLE  0x04
#define CMD_RESET      0x10

/* 中断 */
#define ISR_TOK  0x04
#define ISR_ROK  0x01

/* 接收配置 */
#define RCR_ACCEPT_ALL 0x0F

/* 缓冲区 */
#define RX_BUFFER_SIZE  8192
#define TX_BUFFER_SIZE  1536
#define RX_BUF_ADDR     0x100000  /* 物理地址 1MB */

/* 初始化 RTL8139 */
int  rtl8139_init(void);

/* 获取 MAC 地址 */
void rtl8139_get_mac(uint8_t *mac);

/* 发送数据包 */
int  rtl8139_send(uint8_t *data, int len);

/* 接收数据包 (非阻塞, 返回 0=无数据, >0=数据长度) */
int  rtl8139_recv(uint8_t *buffer, int max_len);

/* PCI 配置空间读取 */
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);

/* 扫描 PCI 找到 RTL8139 */
int  pci_find_rtl8139(uint8_t *bus, uint8_t *slot, uint8_t *func);

#endif /* _RTL8139_H */