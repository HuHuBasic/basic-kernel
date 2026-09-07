/* ata.c — ATA PIO 磁盘驱动 */

#include "kernel.h"

#define ATA_PRIMARY_DATA     0x1F0
#define ATA_PRIMARY_ERR      0x1F1
#define ATA_PRIMARY_SECCOUNT 0x1F2
#define ATA_PRIMARY_LBA_LO   0x1F3
#define ATA_PRIMARY_LBA_MID  0x1F4
#define ATA_PRIMARY_LBA_HI   0x1F5
#define ATA_PRIMARY_DRIVE    0x1F6
#define ATA_PRIMARY_CMD      0x1F7
#define ATA_PRIMARY_STATUS   0x1F7

#define ATA_CMD_READ  0x20
#define ATA_CMD_WRITE 0x30
#define ATA_SR_BSY    0x80
#define ATA_SR_DRQ    0x08
#define ATA_SR_ERR    0x01

static int ata_detected = 0;

void ata_init(void) {
    /* 检测主盘 */
    outb(ATA_PRIMARY_DRIVE, 0xA0);
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);
    outb(ATA_PRIMARY_CMD, 0xEC);  /* IDENTIFY */

    uint8 status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {
        vga_write("  [!!] No ATA drive detected\n");
        return;
    }

    /* 等待 BSY 清零 */
    while (inb(ATA_PRIMARY_STATUS) & ATA_SR_BSY) {}

    /* 检查是否有驱动器 */
    if (inb(ATA_PRIMARY_LBA_MID) != 0 || inb(ATA_PRIMARY_LBA_HI) != 0) {
        ata_detected = 1;
        /* 读取 IDENTIFY 数据 (256 字) */
        for (int i = 0; i < 256; i++) inw(ATA_PRIMARY_DATA);
        vga_write("  [OK] ATA drive detected\n");
    } else {
        vga_write("  [!!] No ATA drive detected\n");
    }
}

int ata_read_sector(uint32 lba, uint8 *buf) {
    if (!ata_detected) return -1;

    /* 等待控制器就绪 */
    while (inb(ATA_PRIMARY_STATUS) & ATA_SR_BSY) {}

    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (uint8)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (uint8)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI, (uint8)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_CMD, ATA_CMD_READ);

    /* 等待数据就绪 */
    while (1) {
        uint8 s = inb(ATA_PRIMARY_STATUS);
        if (s & ATA_SR_ERR) return -1;
        if (s & ATA_SR_DRQ) break;
        if (!(s & ATA_SR_BSY)) break;
    }

    /* 读取 256 字 (512 字节) */
    uint16 *buf16 = (uint16*)buf;
    for (int i = 0; i < 256; i++)
        buf16[i] = inw(ATA_PRIMARY_DATA);

    return 0;
}

int ata_write_sector(uint32 lba, const uint8 *buf) {
    if (!ata_detected) return -1;

    while (inb(ATA_PRIMARY_STATUS) & ATA_SR_BSY) {}

    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (uint8)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (uint8)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI, (uint8)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_CMD, ATA_CMD_WRITE);

    while (!(inb(ATA_PRIMARY_STATUS) & ATA_SR_DRQ)) {
        if (inb(ATA_PRIMARY_STATUS) & ATA_SR_ERR) return -1;
    }

    const uint16 *buf16 = (const uint16*)buf;
    for (int i = 0; i < 256; i++)
        outw(ATA_PRIMARY_DATA, buf16[i]);

    /* 等待写入完成 */
    while (inb(ATA_PRIMARY_STATUS) & ATA_SR_BSY) {}

    return 0;
}