# Makefile — Basic Kernel v2.0 构建系统

ASM = nasm
CC  = gcc
LD  = ld

CFLAGS   = -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector \
           -Wall -Wextra -O2 -std=c99 -Isrc
LDFLAGS  = -m elf_i386 -T src/linker.ld -nostdlib
ASFLAGS  = -f elf32

SRC_DIR   = src
BUILD_DIR = build

OBJS = $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/vga.o \
       $(BUILD_DIR)/lib.o $(BUILD_DIR)/io.o $(BUILD_DIR)/interrupt.o \
       $(BUILD_DIR)/mm.o $(BUILD_DIR)/paging.o $(BUILD_DIR)/ata.o \
       $(BUILD_DIR)/fat32.o $(BUILD_DIR)/shell.o

TARGET = $(BUILD_DIR)/kernel.bin

.PHONY: all clean iso run

all: $(TARGET) iso

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot.o: $(SRC_DIR)/boot.asm | $(BUILD_DIR)
	$(ASM) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

iso: $(TARGET)
	cp $(TARGET) iso/boot/kernel.bin
	grub-mkrescue -o $(BUILD_DIR)/basic-kernel.iso iso 2>/dev/null || \
	xorriso -as mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot \
		-boot-load-size 4 -boot-info-table -o $(BUILD_DIR)/basic-kernel.iso iso 2>/dev/null || \
	echo "ISO creation skipped (install grub-mkrescue or xorriso)"

run: iso
	qemu-system-i386 -cdrom $(BUILD_DIR)/basic-kernel.iso -m 128M -no-reboot

clean:
	rm -rf $(BUILD_DIR)