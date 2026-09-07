/* kernel.h — Basic Kernel 公共头文件 */

#ifndef KERNEL_H
#define KERNEL_H

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;
typedef int            int32;

#define NULL ((void*)0)

/* VGA */
#define VGA_ADDR   0xB8000
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

enum vga_color {
    BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LGRAY,
    DGRAY, LBLUE, LGREEN, LCYAN, LRED, LMAGENTA, YELLOW, WHITE
};

void vga_clear(void);
void vga_putchar(char c);
void vga_write(const char *s);
void vga_write_color(const char *s, uint8 fg);
void vga_dec(uint32 n);
void vga_hex(uint32 n);
void vga_set_color(uint8 fg, uint8 bg);
uint8 make_color(uint8 fg, uint8 bg);

/* 中断 */
void pic_remap(void);
void idt_install(void);
void idt_set_gate(int num, uint32 base, uint16 sel, uint8 flags);
void irq_init(void);
void sti(void);
void cli(void);
void interrupt_init(void);

/* 内存 */
#define PAGE_SIZE 4096
void  mm_init(uint32 mem_upper_kb);
void* pmm_alloc_page(void);
void  pmm_free_page(void *addr);
void  paging_init(void);
void* kmalloc(uint32 size);
void  kfree(void *ptr);
uint32 mm_get_free(void);

/* 磁盘 */
void ata_init(void);
int  ata_read_sector(uint32 lba, uint8 *buf);
int  ata_write_sector(uint32 lba, const uint8 *buf);

/* FAT32 */
typedef struct fat32_fs fat32_fs_t;
typedef struct fat32_file fat32_file_t;

struct fat32_file {
    char     name[256];
    uint32   size;
    uint32   cluster;
    uint32   cur_cluster;
    uint32   cur_offset;
    uint8    is_dir;
    fat32_fs_t *fs;
};

fat32_fs_t* fat32_mount(void);
int         fat32_readdir(fat32_fs_t *fs, const char *path, fat32_file_t *files, int max);
fat32_file_t* fat32_open(fat32_fs_t *fs, const char *path);
int         fat32_read(fat32_file_t *f, void *buf, uint32 size);
void        fat32_close(fat32_file_t *f);
extern fat32_fs_t *mounted_fs;

/* Shell */
void shell_init(void);
void shell_run(void);
void shell_exec(const char *cmd);

/* 字符串 */
int  strcmp(const char *a, const char *b);
int  strncmp(const char *a, const char *b, int n);
void memcpy(void *d, const void *s, uint32 n);
void memset(void *d, uint8 v, uint32 n);

/* 端口 I/O */
void outb(uint16 port, uint8 val);
uint8 inb(uint16 port);
void outw(uint16 port, uint16 val);
uint16 inw(uint16 port);
void io_wait(void);

#endif