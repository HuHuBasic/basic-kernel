#!/bin/bash
# ============================================================
#  Basic Kernel - 安装到电脑脚本
#  支持: 制作启动U盘 / 添加到GRUB双启动
# ============================================================

set -e

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

KERNEL_BIN="kernel.bin"
GRUB_CFG="grub.cfg"
ISO_FILE="basic-kernel.iso"
GRUB_ENTRY_NAME="Basic Kernel"

show_banner() {
    echo -e "${CYAN}"
    echo "  ╔══════════════════════════════════════════╗"
    echo "  ║       Basic Kernel - 安装到电脑          ║"
    echo "  ║   制作启动U盘 / 添加GRUB双启动条目       ║"
    echo "  ╚══════════════════════════════════════════╝"
    echo -e "${NC}"
}

check_root() {
    if [[ "$EUID" -ne 0 ]]; then
        echo -e "${RED}此操作需要 root 权限，请使用 sudo 运行${NC}"
        echo -e "  ${CYAN}→${NC} sudo ./install-to-pc.sh"
        exit 1
    fi
}

# ==========================================
# 方法一: 制作启动 U 盘
# ==========================================
make_usb() {
    echo -e "${BOLD}>>> 制作启动 U 盘${NC}"
    echo ""

    # 列出可用磁盘
    echo -e "${YELLOW}可用磁盘列表:${NC}"
    echo ""
    lsblk -d -o NAME,SIZE,MODEL,TRAN | grep -v "loop\|sr0" || true
    echo ""

    echo -e "${RED}${BOLD}⚠ 警告: 此操作会清空目标 U 盘的所有数据!${NC}"
    echo ""
    read -p "请输入目标 U 盘设备名 (例如 sdb, nvme0n1): " USB_DEV

    if [[ -z "$USB_DEV" ]]; then
        echo -e "${RED}已取消${NC}"
        return
    fi

    USB_PATH="/dev/$USB_DEV"

    if [[ ! -b "$USB_PATH" ]]; then
        echo -e "${RED}✗${NC} 设备 $USB_PATH 不存在"
        return
    fi

    echo ""
    echo -e "目标设备: ${YELLOW}$USB_PATH${NC}"
    lsblk "$USB_PATH"
    echo ""

    read -p "确认要清空并写入 $USB_PATH ? (输入 YES 确认): " confirm
    if [[ "$confirm" != "YES" ]]; then
        echo -e "${YELLOW}已取消${NC}"
        return
    fi

    echo ""
    echo -e "${BOLD}>>> 写入 ISO 到 U 盘...${NC}"

    # 使用 dd 写入 ISO
    echo -e "  ${CYAN}→${NC} 正在写入 $USB_PATH ..."
    dd if="$ISO_FILE" of="$USB_PATH" bs=4M status=progress conv=fsync

    echo ""
    echo -e "  ${CYAN}→${NC} 同步数据..."
    sync

    echo ""
    echo -e "${GREEN}${BOLD}✓ 启动 U 盘制作完成!${NC}"
    echo ""
    echo -e "  ${BOLD}使用方法:${NC}"
    echo -e "  1. 将 U 盘插入目标电脑"
    echo -e "  2. 开机时按 ${CYAN}F2/F12/Del/Esc${NC} 进入 BIOS 启动菜单"
    echo -e "  3. 选择从 U 盘启动"
    echo -e "  4. 在 GRUB 菜单中选择 ${CYAN}Basic Kernel${NC}"
}

# ==========================================
# 方法二: 添加到系统 GRUB 菜单 (双启动)
# ==========================================
add_grub_entry() {
    echo -e "${BOLD}>>> 添加 GRUB 双启动条目${NC}"
    echo ""

    # 检测 GRUB 版本
    if [[ -d "/boot/grub" ]]; then
        GRUB_DIR="/boot/grub"
    elif [[ -d "/boot/grub2" ]]; then
        GRUB_DIR="/boot/grub2"
    else
        echo -e "${RED}✗${NC} 未找到 GRUB 目录，你的系统可能使用其他引导程序"
        return
    fi

    echo -e "  ${GREEN}✓${NC} 检测到 GRUB: $GRUB_DIR"

    # 创建内核目录
    KERNEL_DEST="/boot/basic-kernel"
    echo -e "  ${CYAN}→${NC} 创建 $KERNEL_DEST"
    mkdir -p "$KERNEL_DEST"

    # 复制内核文件
    echo -e "  ${CYAN}→${NC} 复制内核文件..."
    cp "$KERNEL_BIN" "$KERNEL_DEST/"
    cp "$GRUB_CFG" "$KERNEL_DEST/"

    # 创建 GRUB 自定义菜单
    CUSTOM_CFG="/etc/grub.d/40_basic-kernel"

    echo -e "  ${CYAN}→${NC} 创建 GRUB 菜单条目..."

    cat > "$CUSTOM_CFG" << 'GRUBEOF'
#!/bin/sh
exec tail -n +3 $0
# Basic Kernel 启动条目
menuentry "Basic Kernel (x86 教学操作系统)" {
    insmod part_msdos
    insmod ext2
    set root='hd0,msdos1'
    multiboot /boot/basic-kernel/kernel.bin
    boot
}
GRUBEOF

    chmod +x "$CUSTOM_CFG"

    echo ""
    echo -e "${BOLD}正在更新 GRUB 配置...${NC}"

    if command -v update-grub &> /dev/null; then
        update-grub
    elif command -v grub2-mkconfig &> /dev/null; then
        grub2-mkconfig -o "$GRUB_DIR/grub.cfg"
    elif command -v grub-mkconfig &> /dev/null; then
        grub-mkconfig -o "$GRUB_DIR/grub.cfg"
    else
        echo -e "${YELLOW}!${NC} 请手动更新 GRUB:"
        echo -e "  grub-mkconfig -o $GRUB_DIR/grub.cfg"
    fi

    echo ""
    echo -e "${GREEN}${BOLD}✓ GRUB 双启动条目添加完成!${NC}"
    echo ""
    echo -e "  ${BOLD}使用方法:${NC}"
    echo -e "  1. 重启电脑"
    echo -e "  2. 在 GRUB 菜单中选择 ${CYAN}Basic Kernel (x86 教学操作系统)${NC}"
    echo -e "  3. 进入内核 Shell"
    echo ""
    echo -e "  ${BOLD}如需卸载:${NC}"
    echo -e "  sudo rm -rf /boot/basic-kernel"
    echo -e "  sudo rm /etc/grub.d/40_basic-kernel"
    echo -e "  sudo update-grub"
}

# ==========================================
# 方法三: 安装到硬盘分区
# ==========================================
install_to_partition() {
    echo -e "${BOLD}>>> 安装到独立分区${NC}"
    echo ""

    echo -e "${YELLOW}可用分区列表:${NC}"
    echo ""
    lsblk -o NAME,SIZE,FSTYPE,LABEL,MOUNTPOINT | grep -v "loop" || true
    echo ""

    echo -e "${RED}${BOLD}⚠ 警告: 此操作会覆盖目标分区!${NC}"
    echo ""
    read -p "请输入目标分区 (例如 sda3, nvme0n1p3): " TARGET_PART

    if [[ -z "$TARGET_PART" ]]; then
        echo -e "${RED}已取消${NC}"
        return
    fi

    PART_PATH="/dev/$TARGET_PART"

    if [[ ! -b "$PART_PATH" ]]; then
        echo -e "${RED}✗${NC} 分区 $PART_PATH 不存在"
        return
    fi

    echo ""
    read -p "确认要覆盖 $PART_PATH ? (输入 YES 确认): " confirm
    if [[ "$confirm" != "YES" ]]; then
        echo -e "${YELLOW}已取消${NC}"
        return
    fi

    # 格式化分区为 ext2
    echo -e "  ${CYAN}→${NC} 格式化 $PART_PATH 为 ext2..."
    mkfs.ext2 -F "$PART_PATH"

    # 挂载
    MOUNT_POINT="/mnt/basic-kernel-install"
    mkdir -p "$MOUNT_POINT"
    mount "$PART_PATH" "$MOUNT_POINT"

    # 创建 GRUB 目录结构
    echo -e "  ${CYAN}→${NC} 安装 GRUB 引导..."
    mkdir -p "$MOUNT_POINT/boot/grub"

    cp "$KERNEL_BIN" "$MOUNT_POINT/boot/"
    cp "$GRUB_CFG" "$MOUNT_POINT/boot/grub/"

    # 安装 GRUB
    DISK=$(echo "$PART_PATH" | sed 's/[0-9]*$//' | sed 's/p$//')
    echo -e "  ${CYAN}→${NC} 安装 GRUB 到 $DISK"

    if command -v grub-install &> /dev/null; then
        grub-install --root-directory="$MOUNT_POINT" --no-floppy --target=i386-pc "$DISK"
    elif command -v grub2-install &> /dev/null; then
        grub2-install --root-directory="$MOUNT_POINT" --no-floppy --target=i386-pc "$DISK"
    else
        echo -e "${YELLOW}!${NC} 请手动安装 GRUB"
    fi

    umount "$MOUNT_POINT"
    rmdir "$MOUNT_POINT"

    echo ""
    echo -e "${GREEN}${BOLD}✓ 安装到分区完成!${NC}"
    echo ""
    echo -e "  重启后从该分区启动即可进入 Basic Kernel"
}

# ==========================================
# 方法四: 快速安装 kexec 热启动
# ==========================================
kexec_boot() {
    echo -e "${BOLD}>>> kexec 热启动 (无需重启 BIOS)${NC}"
    echo ""

    if ! command -v kexec &> /dev/null; then
        echo -e "${YELLOW}!${NC} 需要安装 kexec-tools"
        echo -e "  ${CYAN}→${NC} sudo apt install kexec-tools"
        read -p "是否现在安装? (y/n): " install_kexec
        if [[ "$install_kexec" == "y" ]]; then
            apt install -y kexec-tools
        else
            return
        fi
    fi

    echo -e "${RED}${BOLD}⚠ 注意: kexec 会直接替换当前内核，请保存所有工作!${NC}"
    echo ""
    read -p "确认要启动 Basic Kernel? (输入 YES 确认): " confirm

    if [[ "$confirm" != "YES" ]]; then
        echo -e "${YELLOW}已取消${NC}"
        return
    fi

    echo -e "  ${CYAN}→${NC} 正在加载 Basic Kernel..."
    kexec --type="multiboot" --load "$KERNEL_BIN"
    echo -e "  ${CYAN}→${NC} 正在启动..."
    kexec -e
}

# ==========================================
# 主菜单
# ==========================================
show_menu() {
    echo ""
    echo -e "${BOLD}请选择安装方式:${NC}"
    echo ""
    echo -e "  ${CYAN}1)${NC} 制作启动 U 盘 (推荐)"
    echo -e "  ${CYAN}2)${NC} 添加到 GRUB 双启动菜单"
    echo -e "  ${CYAN}3)${NC} 安装到独立硬盘分区"
    echo -e "  ${CYAN}4)${NC} kexec 热启动 (无需重启 BIOS)"
    echo -e "  ${CYAN}5)${NC} 退出"
    echo ""

    read -p "请输入选项 [1-5]: " choice

    case "$choice" in
        1) make_usb ;;
        2) add_grub_entry ;;
        3) install_to_partition ;;
        4) kexec_boot ;;
        5) echo -e "${CYAN}再见!${NC}"; exit 0 ;;
        *) echo -e "${RED}无效选项${NC}"; show_menu ;;
    esac
}

# ==========================================
# 主流程
# ==========================================
main() {
    show_banner

    # 检查是否在项目目录
    if [[ ! -f "$KERNEL_BIN" ]]; then
        echo -e "${YELLOW}!${NC} 未找到 kernel.bin，正在编译..."
        make clean &> /dev/null
        make &> /dev/null
        echo -e "  ${GREEN}✓${NC} 编译完成"
    fi

    # 检查 ISO 是否存在 (用于 U 盘制作)
    if [[ ! -f "$ISO_FILE" ]]; then
        echo -e "${YELLOW}!${NC} 未找到 ISO 文件，正在生成..."
        make iso &> /dev/null || true
        if [[ -f "$ISO_FILE" ]]; then
            echo -e "  ${GREEN}✓${NC} ISO 生成完成"
        else
            echo -e "  ${YELLOW}!${NC} ISO 生成失败（U盘模式需要安装 grub-mkrescue/xorriso）"
        fi
    fi

    show_menu
}

main