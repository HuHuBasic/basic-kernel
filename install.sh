#!/bin/bash
# ============================================================
#  Basic Kernel - 一键安装脚本 (QEMU 模拟运行)
#  自动检测系统、安装依赖、编译内核、运行
#  如需安装到真机: sudo ./install-to-pc.sh
# ============================================================

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # 无颜色

# 横幅
show_banner() {
    echo -e "${CYAN}"
    echo "  ╔══════════════════════════════════════════╗"
    echo "  ║          Basic Kernel 安装程序           ║"
    echo "  ║        x86 32-bit 教学操作系统内核        ║"
    echo "  ║   模拟运行: ./install.sh                  ║"
    echo "  ║   真机安装: sudo ./install-to-pc.sh       ║"
    echo "  ╚══════════════════════════════════════════╝"
    echo -e "${NC}"
}

# 检查命令是否存在
check_cmd() {
    command -v "$1" &> /dev/null
}

# 检测操作系统
detect_os() {
    echo -e "${BOLD}>>> 检测操作系统...${NC}"

    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        OS="linux"
        if check_cmd apt; then
            PKG_MANAGER="apt"
        elif check_cmd pacman; then
            PKG_MANAGER="pacman"
        elif check_cmd dnf; then
            PKG_MANAGER="dnf"
        elif check_cmd yum; then
            PKG_MANAGER="yum"
        elif check_cmd zypper; then
            PKG_MANAGER="zypper"
        else
            PKG_MANAGER="unknown"
        fi
        echo -e "  ${GREEN}✓${NC} 检测到 Linux (${PKG_MANAGER})"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
        PKG_MANAGER="brew"
        echo -e "  ${GREEN}✓${NC} 检测到 macOS"
    else
        echo -e "  ${RED}✗${NC} 不支持的操作系统: $OSTYPE"
        exit 1
    fi
}

# 安装依赖
install_deps() {
    echo -e "${BOLD}>>> 检查并安装依赖...${NC}"

    local missing=""

    # 检查 GCC (32-bit)
    if ! check_cmd gcc; then
        missing="$missing gcc"
    fi

    # 检查 ld
    if ! check_cmd ld; then
        missing="$missing binutils"
    fi

    # 检查 QEMU
    if ! check_cmd qemu-system-i386; then
        missing="$missing qemu"
    fi

    if [[ -z "$missing" ]]; then
        echo -e "  ${GREEN}✓${NC} 所有依赖已安装"
        return
    fi

    echo -e "  ${YELLOW}!${NC} 需要安装:${missing}"

    case "$PKG_MANAGER" in
        apt)
            echo -e "  ${CYAN}→${NC} 执行: sudo apt install gcc-multilib qemu-system-x86 make"
            sudo apt update
            sudo apt install -y gcc-multilib qemu-system-x86 make
            ;;
        pacman)
            echo -e "  ${CYAN}→${NC} 执行: sudo pacman -S gcc qemu make"
            sudo pacman -S --noconfirm gcc qemu make
            ;;
        dnf|yum)
            echo -e "  ${CYAN}→${NC} 执行: sudo dnf install gcc qemu make"
            sudo $PKG_MANAGER install -y gcc qemu-system-x86 make glibc-devel.i686
            ;;
        zypper)
            echo -e "  ${CYAN}→${NC} 执行: sudo zypper install gcc qemu make"
            sudo zypper install -y gcc qemu-x86 make
            ;;
        brew)
            echo -e "  ${CYAN}→${NC} 执行: brew install qemu"
            brew install qemu
            ;;
        *)
            echo -e "  ${RED}✗${NC} 请手动安装以下依赖:${missing}"
            echo -e "    - GCC (支持 32-bit 编译)"
            echo -e "    - GNU Binutils (ld, as)"
            echo -e "    - QEMU (qemu-system-i386)"
            exit 1
            ;;
    esac

    echo -e "  ${GREEN}✓${NC} 依赖安装完成"
}

# 编译内核
build_kernel() {
    echo -e "${BOLD}>>> 编译内核...${NC}"

    make clean &> /dev/null

    if make 2>&1 | tail -5; then
        echo -e "  ${GREEN}✓${NC} 编译成功!"
        local size=$(ls -lh kernel.bin | awk '{print $5}')
        echo -e "  ${CYAN}→${NC} 内核文件: kernel.bin (${size})"
    else
        echo -e "  ${RED}✗${NC} 编译失败，请检查错误信息"
        exit 1
    fi
}

# 运行内核
run_kernel() {
    echo ""
    echo -e "${BOLD}>>> 启动内核...${NC}"
    echo -e "  ${YELLOW}!${NC} QEMU 窗口即将打开，按 Ctrl+Alt 可释放鼠标"
    echo -e "  ${YELLOW}!${NC} 在 Shell 中输入 'help' 查看可用命令"
    echo -e "  ${YELLOW}!${NC} 关闭 QEMU 窗口即可退出"
    echo ""

    sleep 2
    make run
}

# 生成 ISO
build_iso() {
    echo -e "${BOLD}>>> 生成 ISO 镜像...${NC}"

    if ! check_cmd grub-mkrescue && ! check_cmd grub2-mkrescue && ! check_cmd xorriso; then
        echo -e "  ${YELLOW}!${NC} 需要 grub-mkrescue 或 xorriso 来生成 ISO"
        echo -e "  ${CYAN}→${NC} 尝试安装..."

        case "$PKG_MANAGER" in
            apt)
                sudo apt install -y grub-pc-bin xorriso 2>/dev/null || true
                ;;
            brew)
                brew install xorriso 2>/dev/null || true
                ;;
        esac
    fi

    if make iso 2>&1; then
        echo -e "  ${GREEN}✓${NC} ISO 生成成功!"
        echo -e "  ${CYAN}→${NC} 文件: basic-kernel.iso"
        echo -e "  ${CYAN}→${NC} 可写入 U 盘或用 VirtualBox 启动"
    fi
}

# 交互式菜单
show_menu() {
    echo ""
    echo -e "${BOLD}请选择操作:${NC}"
    echo ""
    echo -e "  ${CYAN}1)${NC} 一键安装并运行 (推荐)"
    echo -e "  ${CYAN}2)${NC} 仅编译，不运行"
    echo -e "  ${CYAN}3)${NC} 运行已编译的内核"
    echo -e "  ${CYAN}4)${NC} 生成 ISO 镜像"
    echo -e "  ${CYAN}5)${NC} 清理编译产物"
    echo -e "  ${CYAN}6)${NC} 退出"
    echo ""

    read -p "请输入选项 [1-6]: " choice

    case "$choice" in
        1)
            install_deps
            build_kernel
            run_kernel
            ;;
        2)
            install_deps
            build_kernel
            echo ""
            echo -e "${GREEN}编译完成! 执行 'make run' 运行内核${NC}"
            ;;
        3)
            if [[ ! -f "kernel.bin" ]]; then
                echo -e "${YELLOW}!${NC} 未找到 kernel.bin，先编译..."
                build_kernel
            fi
            run_kernel
            ;;
        4)
            install_deps
            build_kernel
            build_iso
            ;;
        5)
            echo -e "${BOLD}>>> 清理...${NC}"
            make clean
            echo -e "  ${GREEN}✓${NC} 清理完成"
            ;;
        6)
            echo -e "${CYAN}再见!${NC}"
            exit 0
            ;;
        *)
            echo -e "${RED}无效选项${NC}"
            show_menu
            ;;
    esac
}

# 命令行参数处理
handle_args() {
    case "${1:-}" in
        --auto|-a)
            install_deps
            build_kernel
            run_kernel
            ;;
        --build|-b)
            install_deps
            build_kernel
            ;;
        --run|-r)
            run_kernel
            ;;
        --iso|-i)
            install_deps
            build_kernel
            build_iso
            ;;
        --clean|-c)
            make clean
            echo -e "${GREEN}✓${NC} 清理完成"
            ;;
        --help|-h)
            echo "用法: ./install.sh [选项]"
            echo ""
            echo "选项:"
            echo "  (无参数)     交互式菜单"
            echo "  -a, --auto   一键安装并运行"
            echo "  -b, --build  仅安装依赖并编译"
            echo "  -r, --run    仅运行已编译的内核"
            echo "  -i, --iso    编译并生成 ISO 镜像"
            echo "  -c, --clean  清理编译产物"
            echo "  -h, --help   显示此帮助"
            ;;
        *)
            show_menu
            ;;
    esac
}

# ---- 主流程 ----
main() {
    show_banner

    detect_os

    # 检查是否在项目目录中
    if [[ ! -f "Makefile" ]] || [[ ! -f "linker.ld" ]]; then
        echo -e "${RED}✗${NC} 请在 basic-kernel 项目目录中运行此脚本"
        exit 1
    fi

    handle_args "$@"
}

main "$@"