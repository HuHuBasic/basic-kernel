#!/bin/bash
# ============================================================
#  HU basic - 安装包制作脚本
#  生成统一的 .bak 安装包，包含内核 + 源码 + 工具
# ============================================================

set -e

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

show_banner() {
    echo -e "${CYAN}"
    echo "  ╔══════════════════════════════════════════╗"
    echo "  ║       HU basic - 安装包制作工具          ║"
    echo "  ║     生成 .bak 统一安装包                  ║"
    echo "  ╚══════════════════════════════════════════╝"
    echo -e "${NC}"
}

# ---- 检查依赖 ----
check_deps() {
    echo -e "${BOLD}>>> 检查依赖...${NC}"
    local ok=1

    for cmd in gcc as ld tar gzip; do
        if ! command -v "$cmd" &> /dev/null; then
            echo -e "  ${RED}✗${NC} 缺少: $cmd"
            ok=0
        fi
    done

    if [[ "$ok" -eq 0 ]]; then
        echo -e "${RED}请先安装缺失的依赖${NC}"
        exit 1
    fi
    echo -e "  ${GREEN}✓${NC} 依赖检查通过"
}

# ---- 编译内核 ----
build_kernel() {
    echo -e "${BOLD}>>> 编译内核...${NC}"

    if [[ -f "$SCRIPT_DIR/Makefile" ]]; then
        cd "$SCRIPT_DIR"
        make clean &> /dev/null
        make &> /dev/null
        echo -e "  ${GREEN}✓${NC} 编译完成: kernel.bin ($(ls -lh kernel.bin | awk '{print $5}'))"
    else
        echo -e "  ${YELLOW}!${NC} 未找到 Makefile，跳过编译"
    fi
}

# ---- 生成 ISO ----
build_iso() {
    echo -e "${BOLD}>>> 生成 ISO 镜像...${NC}"

    if [[ -f "$SCRIPT_DIR/Makefile" ]]; then
        cd "$SCRIPT_DIR"
        make iso &> /dev/null 2>&1 || true
        if [[ -f "basic-kernel.iso" ]]; then
            echo -e "  ${GREEN}✓${NC} ISO 生成完成"
        else
            echo -e "  ${YELLOW}!${NC} ISO 生成失败（需要 grub-mkrescue/xorriso），跳过"
        fi
    fi
}

# ---- 打包为 .bak ----
package_bak() {
    local OUT_NAME="HU-basic-v1.1.bak"
    local TMP_DIR="/tmp/hu-basic-pkg"

    echo -e "${BOLD}>>> 打包为 .bak 安装包...${NC}"

    # 清理旧临时目录
    rm -rf "$TMP_DIR"
    mkdir -p "$TMP_DIR/boot"
    mkdir -p "$TMP_DIR/src"
    mkdir -p "$TMP_DIR/tools"

    # 复制内核二进制
    if [[ -f "$SCRIPT_DIR/kernel.bin" ]]; then
        cp "$SCRIPT_DIR/kernel.bin" "$TMP_DIR/boot/"
        echo -e "  ${CYAN}→${NC} 复制 kernel.bin"
    fi

    # 复制 ISO
    if [[ -f "$SCRIPT_DIR/basic-kernel.iso" ]]; then
        cp "$SCRIPT_DIR/basic-kernel.iso" "$TMP_DIR/boot/"
        echo -e "  ${CYAN}→${NC} 复制 basic-kernel.iso"
    fi

    # 复制 GRUB 配置
    if [[ -f "$SCRIPT_DIR/grub.cfg" ]]; then
        cp "$SCRIPT_DIR/grub.cfg" "$TMP_DIR/boot/grub.cfg"
        echo -e "  ${CYAN}→${NC} 复制 grub.cfg"
    fi

    # 复制源码
    if [[ -d "$SCRIPT_DIR/src" ]]; then
        cp -r "$SCRIPT_DIR/src" "$TMP_DIR/"
        echo -e "  ${CYAN}→${NC} 复制源码目录 src/"
    fi

    # 复制 Makefile 和链接脚本
    for f in Makefile linker.ld; do
        if [[ -f "$SCRIPT_DIR/$f" ]]; then
            cp "$SCRIPT_DIR/$f" "$TMP_DIR/"
            echo -e "  ${CYAN}→${NC} 复制 $f"
        fi
    done

    # 复制安装脚本
    for f in install.sh install-to-pc.sh; do
        if [[ -f "$SCRIPT_DIR/$f" ]]; then
            cp "$SCRIPT_DIR/$f" "$TMP_DIR/tools/"
            chmod +x "$TMP_DIR/tools/$f"
            echo -e "  ${CYAN}→${NC} 复制 $f"
        fi
    done

    # 复制开发手册
    if [[ -f "$SCRIPT_DIR/APPDEV.BAK" ]]; then
        cp "$SCRIPT_DIR/APPDEV.BAK" "$TMP_DIR/"
        echo -e "  ${CYAN}→${NC} 复制 APPDEV.BAK"
    fi

    # 复制 README
    if [[ -f "$SCRIPT_DIR/README.md" ]]; then
        cp "$SCRIPT_DIR/README.md" "$TMP_DIR/"
        echo -e "  ${CYAN}→${NC} 复制 README.md"
    fi

    # 生成包信息文件
    cat > "$TMP_DIR/package.info" << EOF
name=HU-basic
version=1.1
arch=x86-32
type=os-kernel
date=$(date '+%Y-%m-%d')
packager=HU-basic-build
description=一个用于学习的 x86 32-bit 操作系统内核
features=GDT/IDT/VGA/Keyboard/PIT/Memory/Apps/Desktop/Shell
EOF

    # 生成安装器入口
    cat > "$TMP_DIR/install.sh" << 'ENTRYEOF'
#!/bin/bash
# HU basic 安装器 (从 .bak 包中提取)
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
echo "============================================"
echo "  HU basic v1.1 - 安装器"
echo "============================================"
echo ""
echo "1) 编译并运行 (QEMU 模拟)"
echo "2) 安装到真机"
echo "3) 查看文档"
echo "4) 退出"
echo ""
read -p "请选择 [1-4]: " choice

case "$choice" in
    1)
        if [[ -f "$SCRIPT_DIR/tools/install.sh" ]]; then
            bash "$SCRIPT_DIR/tools/install.sh"
        else
            echo "错误: 未找到 install.sh"
        fi
        ;;
    2)
        if [[ -f "$SCRIPT_DIR/tools/install-to-pc.sh" ]]; then
            sudo bash "$SCRIPT_DIR/tools/install-to-pc.sh"
        else
            echo "错误: 未找到 install-to-pc.sh"
        fi
        ;;
    3)
        if [[ -f "$SCRIPT_DIR/APPDEV.BAK" ]]; then
            cat "$SCRIPT_DIR/APPDEV.BAK" | head -80
        fi
        ;;
    4)
        echo "再见!"
        exit 0
        ;;
    *)
        echo "无效选项"
        ;;
esac
ENTRYEOF
    chmod +x "$TMP_DIR/install.sh"

    # 打包
    cd "$TMP_DIR"
    tar -czf "/tmp/hu-basic-payload.tar.gz" .
    cd "$SCRIPT_DIR"

    # 生成自解压 .bak 文件
    cat > "$OUT_NAME" << 'BAKHEAD'
#!/bin/bash
# ============================================================
#  HU basic 安装包 (.bak)
#  运行此文件即可安装: bash HU-basic-v1.1.bak
# ============================================================
echo "============================================"
echo "  HU basic v1.1 - 安装包"
echo "  正在解压..."
echo "============================================"

TMPDIR=$(mktemp -d)
ARCHIVE=$(awk '/^__ARCHIVE__$/ {print NR+1; exit}' "$0")
tail -n +$ARCHIVE "$0" | tar -xz -C "$TMPDIR"

echo "解压完成: $TMPDIR"
echo ""
cd "$TMPDIR" && bash install.sh

# 清理
rm -rf "$TMPDIR"
exit 0
__ARCHIVE__
BAKHEAD

    cat "/tmp/hu-basic-payload.tar.gz" >> "$OUT_NAME"
    chmod +x "$OUT_NAME"

    rm -rf "$TMPDIR" "/tmp/hu-basic-payload.tar.gz"

    echo -e "  ${GREEN}${BOLD}✓${NC} 安装包生成完成!"
    echo -e "  ${CYAN}→${NC} 文件: $(pwd)/$OUT_NAME"
    echo -e "  ${CYAN}→${NC} 大小: $(ls -lh "$OUT_NAME" | awk '{print $5}')"
    echo ""
    echo -e "  ${BOLD}使用方式:${NC}"
    echo -e "  bash $OUT_NAME"
}

# ---- 主流程 ----
main() {
    show_banner
    check_deps
    build_kernel
    build_iso
    package_bak
}

main "$@"