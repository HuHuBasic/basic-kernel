/*
 * rtl8139.c - RTL8139 网卡驱动实现
 * 支持 PCI 扫描 + 寄存器操作 + 发送/接收
 */
#include "rtl8139.h"
#include "../ports.h"
#include "../vga.h"
#include "../string.h"
#include "../memory.h"
#include "../timer.h"

/* 网卡 I/O 基地址 */
static uint16_t nic_io_base = 0;
static uint8_t  nic_mac_addr[6];
static int      nic_found = 0;

/* 接收缓冲区 */
static uint8_t *rx_buffer = NULL;
static int     rx_offset = 0;
static uint8_t *tx_buffer[4] = {NULL, NULL, NULL, NULL};

/* ---- PCI 配置空间访问 ---- */
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t addr = (uint32_t)(0x80000000 |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFC));
    outl(0xCF8, addr);
    return inl(0xCFC);
}

void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val)
{
    uint32_t addr = (uint32_t)(0x80000000 |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFC));
    outl(0xCF8, addr);
    outl(0xCFC, val);
}

int pci_find_rtl8139(uint8_t *out_bus, uint8_t *out_slot, uint8_t *out_func)
{
    /* RTL8139: vendor=0x10EC, device=0x8139 */
    for (uint8_t bus = 0; bus < 4; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_config_read(bus, slot, 0, 0);
            uint16_t vendor = id & 0xFFFF;
            uint16_t device = (id >> 16) & 0xFFFF;

            if (vendor == 0x10EC && device == 0x8139) {
                *out_bus  = bus;
                *out_slot = slot;
                *out_func = 0;
                return 1;
            }

            /* 也检查 AMD PCnet (QEMU 默认) */
            if (vendor == 0x1022 && (device == 0x2000)) {
                /* PCnet 有不同的驱动, 这里放行但标记 */
                *out_bus  = bus;
                *out_slot = slot;
                *out_func = 0;
                return 2;  /* 2 = PCnet */
            }
        }
    }
    return 0;
}

int rtl8139_init(void)
{
    uint8_t bus, slot, func;
    int type = pci_find_rtl8139(&bus, &slot, &func);

    if (type == 0) {
        terminal_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_print("[NET] No RTL8139 or PCnet NIC found!\n");
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_print("[NET] Run with: qemu -netdev user,id=n0 -device rtl8139,netdev=n0\n");
        return -1;
    }

    if (type == 2) {
        terminal_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        terminal_print("[NET] PCnet detected (RTL8139 preferred).\n");
        terminal_print("[NET] Use: -device rtl8139,netdev=n0 for best results.\n");
        terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        return -1;
    }

    /* 读取 BAR0 (I/O 基地址) */
    uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
    nic_io_base = bar0 & 0xFFFC;

    /* 启用总线主控和 I/O */
    uint32_t cmd = pci_config_read(bus, slot, func, 0x04);
    cmd |= 0x05;  /* IO + Bus Master */
    pci_config_write(bus, slot, func, 0x04, cmd);

    /* 软件复位 */
    outb(nic_io_base + RTL_CMD, CMD_RESET);
    timer_sleep(10);

    /* 等待复位完成 */
    for (int i = 0; i < 100; i++) {
        if (!(inb(nic_io_base + RTL_CMD) & CMD_RESET)) break;
        timer_sleep(1);
    }

    /* 读取 MAC 地址 */
    for (int i = 0; i < 6; i++) {
        nic_mac_addr[i] = inb(nic_io_base + RTL_IDR0 + i);
    }

    /* 分配并设置接收缓冲区 */
    rx_buffer = (uint8_t *)malloc(RX_BUFFER_SIZE + 16 + 1500);
    if (!rx_buffer) return -1;
    /* 对齐到 16 字节 */
    uint32_t rx_phys = (uint32_t)rx_buffer;
    rx_phys = (rx_phys + 15) & ~15;
    rx_buffer = (uint8_t *)rx_phys;

    outl(nic_io_base + RTL_RBSTART, rx_phys);

    /* 分配发送缓冲区 */
    for (int i = 0; i < 4; i++) {
        tx_buffer[i] = (uint8_t *)malloc(TX_BUFFER_SIZE + 16);
        if (tx_buffer[i]) {
            uint32_t tx_phys = (uint32_t)tx_buffer[i];
            tx_phys = (tx_phys + 15) & ~15;
            tx_buffer[i] = (uint8_t *)tx_phys;
            outl(nic_io_base + RTL_TSAD0 + i * 4, tx_phys);
        }
    }

    /* 配置接收: 接受所有包 */
    outl(nic_io_base + RTL_RCR, RCR_ACCEPT_ALL);

    /* 启用接收和发送 */
    outb(nic_io_base + RTL_CMD, CMD_RX_ENABLE | CMD_TX_ENABLE);

    /* 屏蔽中断 (使用轮询) */
    outw(nic_io_base + RTL_IMR, 0x0000);

    nic_found = 1;
    rx_offset = 0;

    terminal_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    terminal_print("[NET] RTL8139 initialized. MAC: ");
    for (int i = 0; i < 6; i++) {
        terminal_print_hex(nic_mac_addr[i]);
        if (i < 5) terminal_putchar(':');
    }
    terminal_putchar('\n');
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    return 0;
}

void rtl8139_get_mac(uint8_t *mac)
{
    memcpy(mac, nic_mac_addr, 6);
}

int rtl8139_send(uint8_t *data, int len)
{
    if (!nic_found) return -1;
    if (len > TX_BUFFER_SIZE) return -1;

    /* 使用第一个发送描述符 */
    memcpy(tx_buffer[0], data, len);

    outl(nic_io_base + RTL_TSD0, len & 0xFFF);

    /* 等待发送完成 */
    for (int i = 0; i < 1000; i++) {
        uint32_t status = inl(nic_io_base + RTL_TSD0);
        if (status & 0x8000) return len;  /* TOK */
        if (status & 0x4000) return -1;   /* TABT */
        timer_sleep(1);
    }

    return -1; /* 超时 */
}

int rtl8139_recv(uint8_t *buffer, int max_len)
{
    if (!nic_found) return -1;

    /* 读取 CAPR */
    uint16_t capr = inw(nic_io_base + RTL_CAPR);
    uint16_t rx_idx = capr % RX_BUFFER_SIZE;

    if (rx_idx == rx_offset) return 0; /* 无数据 */

    /* 读取包头 */
    uint16_t status = *(uint16_t *)(rx_buffer + rx_offset);
    uint16_t length = *(uint16_t *)(rx_buffer + rx_offset + 2);

    /* 检查 ROK */
    if (!(status & 0x01)) {
        /* 重置 */
        rx_offset = 0;
        outw(nic_io_base + RTL_CAPR, 0);
        return 0;
    }

    if (length > max_len) length = max_len;

    /* 复制数据 (跳过 4 字节包头) */
    memcpy(buffer, rx_buffer + rx_offset + 4, length);

    /* 更新读取偏移 */
    rx_offset = (rx_offset + length + 4 + 3) & ~3;
    rx_offset %= RX_BUFFER_SIZE;
    outw(nic_io_base + RTL_CAPR, rx_offset - 16);
    if (rx_offset < 16) rx_offset += RX_BUFFER_SIZE;

    return length;
}