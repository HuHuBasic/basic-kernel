# ============================================================
#  Basic Kernel - Makefile
#  目标: x86 32-bit ELF, 使用 GRUB Multiboot 引导
# ============================================================

# 交叉编译工具链 (如果没有 i686-elf-* 可以用 gcc -m32 代替)
CC       = gcc
AS       = as
LD       = ld

# 编译标志
CFLAGS   = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
           -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
           -nostartfiles -nodefaultlibs \
           -I./src

ASFLAGS  = --32

LDFLAGS  = -m elf_i386 -T linker.ld -nostdlib

# 源文件
SRC_DIR  = src
C_SRCS   = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/net/*.c)
ASM_SRCS = $(wildcard $(SRC_DIR)/*.s)
OBJS     = $(C_SRCS:.c=.o) $(ASM_SRCS:.s=.o)

# 输出
TARGET   = kernel.bin
ISO_DIR  = isodir
ISO      = basic-kernel.iso

.PHONY: all clean run iso debug

all: $(TARGET)

# 链接内核
$(TARGET): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS)

# 编译 C 文件
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 编译 net/ 子目录 C 文件
$(SRC_DIR)/net/%.o: $(SRC_DIR)/net/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 汇编汇编文件
$(SRC_DIR)/%.o: $(SRC_DIR)/%.s
	$(AS) $(ASFLAGS) $< -o $@

# 生成 ISO 镜像 (用于 QEMU 或 VirtualBox)
iso: $(TARGET)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(TARGET) $(ISO_DIR)/boot/
	cp grub.cfg $(ISO_DIR)/boot/grub/
	grub-mkrescue -o $(ISO) $(ISO_DIR) 2>/dev/null || \
	grub2-mkrescue -o $(ISO) $(ISO_DIR) 2>/dev/null || \
	xorriso -as mkisofs -R -b boot/grub/i386-pc/eltorito.img \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		-o $(ISO) $(ISO_DIR) 2>/dev/null || \
	echo "请手动安装 grub-mkrescue 或 xorriso 来生成 ISO"

# 直接使用 QEMU 运行内核 (不需要 ISO)
run: $(TARGET)
	qemu-system-i386 -kernel $(TARGET) -m 128M -serial stdio

# 运行 ISO
run-iso: iso
	qemu-system-i386 -cdrom $(ISO) -m 128M -serial stdio

# 调试模式
debug: $(TARGET)
	qemu-system-i386 -s -S -kernel $(TARGET) -m 128M &
	gdb -ex "target remote localhost:1234" -ex "symbol-file $(TARGET)"

# 清理
clean:
	rm -f $(SRC_DIR)/*.o $(SRC_DIR)/net/*.o $(TARGET)
	rm -rf $(ISO_DIR) $(ISO)