# Basic Kernel

一个用于学习的 x86 32-bit 操作系统内核，使用 GRUB Multiboot 引导。

## 特性

- **Multiboot 引导** — 兼容 GRUB，可直接被 QEMU 或真机引导
- **GDT** — 全局描述符表，支持内核态和用户态段
- **IDT** — 中断描述符表，处理 CPU 异常和硬件中断
- **VGA 文本模式** — 80x25 字符显示，支持 16 种颜色
- **PS/2 键盘驱动** — 支持 US QWERTY 布局，含 Shift/Caps Lock
- **PIT 定时器** — 可编程间隔定时器，100Hz 时钟
- **基础内存管理** — 首次适配 (First-Fit) 动态内存分配
- **交互式 Shell** — 支持 help/clear/echo/time/mem/about/reboot 命令

## 项目结构

```
basic-kernel/
├── Makefile          # 构建系统
├── linker.ld         # 链接脚本
├── grub.cfg          # GRUB 配置
├── README.md
└── src/
    ├── boot.s        # 汇编启动代码 (Multiboot header + GDT)
    ├── kernel.c      # 内核主入口
    ├── vga.c/h       # VGA 文本模式驱动
    ├── gdt.c/h       # 全局描述符表
    ├── idt.c/h       # 中断描述符表
    ├── isr.c/h       # 中断服务例程 (CPU 异常)
    ├── irq.c/h       # 硬件中断处理
    ├── keyboard.c/h  # PS/2 键盘驱动
    ├── timer.c/h     # PIT 定时器
    ├── memory.c/h    # 内存管理
    ├── shell.c/h     # 交互式 Shell
    ├── string.c/h    # 字符串操作
    ├── ports.c/h     # I/O 端口操作
    └── types.h       # 通用类型定义
```

## 构建要求

- **GCC** (支持 32-bit 编译)
- **GNU Binutils** (as, ld)
- **QEMU** (用于模拟运行)
- **GRUB** (可选，用于生成 ISO 镜像)

## 快速开始

```bash
# 一键安装运行 (推荐)
./install.sh
```

## 安装到电脑 (真机运行)

```bash
sudo ./install-to-pc.sh
```

支持四种安装方式：

| 方式 | 说明 | 需要 |
|------|------|------|
| 制作启动 U 盘 | 写入 ISO 到 U 盘，开机从 U 盘启动 | U 盘 + ISO 文件 |
| GRUB 双启动 | 添加条目到系统 GRUB 菜单 | 已有 Linux + GRUB |
| 安装到独立分区 | 格式化分区并安装 GRUB | 空闲分区 |
| kexec 热启动 | 直接替换当前内核启动 | 已安装 Linux |

## 手动构建

```bash
make              # 编译内核
make run          # QEMU 模拟运行
make iso          # 生成 ISO 镜像
make run-iso      # QEMU 运行 ISO
make clean        # 清理
```

## 使用说明

内核启动后会自动进入交互式 Shell：

```
kernel> help
kernel> echo Hello World
kernel> time
kernel> mem
kernel> about
kernel> clear
kernel> reboot
```

## Shell 命令

| 命令 | 说明 |
|------|------|
| `help` | 显示所有可用命令 |
| `clear` | 清屏 |
| `echo <text>` | 回显文本 |
| `time` | 显示系统运行时间 |
| `mem` | 显示内存使用情况 |
| `about` | 关于本内核 |
| `reboot` | 重启系统 |

## 技术细节

- **内存布局**: 内核加载到 1MB 物理地址
- **栈大小**: 16KB
- **堆大小**: 4MB
- **定时器频率**: 100 Hz
- **IRQ 映射**: IRQ 0-15 → INT 0x20-0x2F

## 许可证

MIT License — 仅供学习使用