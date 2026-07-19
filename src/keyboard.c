/*
 * keyboard.c - PS/2 键盘驱动实现
 * 使用环形缓冲区存储按键 + 特殊键支持
 */
#include "keyboard.h"
#include "ports.h"
#include "irq.h"
#include "vga.h"
#include "idt.h"
#include "string.h"

/* 键盘缓冲区 */
#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile int keyboard_read_index  = 0;
static volatile int keyboard_write_index = 0;

/* 特殊键缓冲区 */
static volatile int keyboard_special_key = 0;

/* 键盘状态 */
static int shift_pressed  = 0;
static int caps_lock      = 0;
static int ctrl_pressed   = 0;
static int ext_scancode   = 0;  /* E0 扩展前缀 */

/* US QWERTY 键盘扫描码映射 (扫描码集 1) */
static const char keymap_normal[] = {
    0,    0x1B, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',  /* 0x00-0x0F */
    '\t', 'q',  'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,    /* 0x10-0x1F */
    'a',  's',  'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,    '\\',     /* 0x20-0x2F */
    'z',  'x',  'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,    '*', 0,    ' ',      /* 0x30-0x3F */
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,       /* 0x40-0x4F */
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,                   /* 0x50-0x5A */
};

static const char keymap_shift[] = {
    0,    0x1B, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A',  'S',  'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,    '|',
    'Z',  'X',  'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,    '*', 0,    ' ',
};

/* 判断是否为字母 */
static inline int is_letter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* 写入缓冲区 */
static void keyboard_buffer_write(char c)
{
    int next = (keyboard_write_index + 1) % KEYBOARD_BUFFER_SIZE;
    if (next != keyboard_read_index) {
        keyboard_buffer[keyboard_write_index] = c;
        keyboard_write_index = next;
    }
}

/* 键盘中断处理 */
static void keyboard_callback(registers_t *regs)
{
    (void)regs;

    uint8_t scancode = inb(0x60);

    /* E0 扩展前缀 */
    if (scancode == 0xE0) {
        ext_scancode = 1;
        return;
    }

    /* 按键释放 */
    if (scancode & 0x80) {
        uint8_t release_code = scancode & 0x7F;
        if (release_code == 0x2A || release_code == 0x36) {
            shift_pressed = 0;
        }
        if (release_code == 0x1D) {
            ctrl_pressed = 0;
        }
        ext_scancode = 0;
        return;
    }

    /* 扩展键 (E0 前缀的) */
    if (ext_scancode) {
        ext_scancode = 0;
        switch (scancode) {
            case 0x48: keyboard_special_key = KEY_UP;    return;
            case 0x50: keyboard_special_key = KEY_DOWN;  return;
            case 0x4B: keyboard_special_key = KEY_LEFT;  return;
            case 0x4D: keyboard_special_key = KEY_RIGHT; return;
            default:   return;
        }
    }

    /* 特殊键 */
    switch (scancode) {
        case 0x2A:  /* 左 Shift 按下 */
        case 0x36:  /* 右 Shift 按下 */
            shift_pressed = 1;
            return;
        case 0x1D:  /* 左 Ctrl 按下 */
            ctrl_pressed = 1;
            return;
        case 0x3A:  /* Caps Lock 切换 */
            caps_lock = !caps_lock;
            return;
        case 0x01:  /* Escape */
            keyboard_buffer_write(0x1B);
            keyboard_special_key = KEY_ESC;
            return;
        case 0x0E:  /* Backspace */
            keyboard_buffer_write('\b');
            return;
        case 0x1C:  /* Enter */
            keyboard_buffer_write('\n');
            keyboard_special_key = KEY_ENTER;
            return;
        case 0x39:  /* Space */
            keyboard_buffer_write(' ');
            return;
        case 0x0F:  /* Tab */
            keyboard_buffer_write('\t');
            keyboard_special_key = KEY_TAB;
            return;
        /* 功能键 */
        case 0x3B: keyboard_special_key = KEY_F1; return;
        case 0x3C: keyboard_special_key = KEY_F2; return;
        case 0x3D: keyboard_special_key = KEY_F3; return;
        case 0x3E: keyboard_special_key = KEY_F4; return;
        case 0x3F: case 0x40: case 0x41: case 0x42:  /* F5-F8 */
        case 0x43: case 0x44: case 0x57: case 0x58:  /* F9-F12 */
            return;
    }

    if (scancode >= sizeof(keymap_normal)) return;

    char c;
    if (shift_pressed) {
        c = keymap_shift[scancode];
    } else {
        c = keymap_normal[scancode];
    }

    /* Caps Lock: 反转字母大小写 */
    if (caps_lock && is_letter(c)) {
        if (c >= 'a' && c <= 'z')
            c = c - 'a' + 'A';
        else if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 'a';
    }

    if (c != 0) {
        keyboard_buffer_write(c);
    }
}

void keyboard_init(void)
{
    memset(keyboard_buffer, 0, KEYBOARD_BUFFER_SIZE);
    keyboard_read_index  = 0;
    keyboard_write_index = 0;
    shift_pressed = 0;
    caps_lock     = 0;
    ctrl_pressed  = 0;

    irq_register_handler(1, keyboard_callback);
}

char keyboard_getchar(void)
{
    if (keyboard_read_index == keyboard_write_index) {
        return 0;  /* 缓冲区为空 */
    }

    char c = keyboard_buffer[keyboard_read_index];
    keyboard_read_index = (keyboard_read_index + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

void keyboard_readline(char *buffer, int max_len)
{
    int pos = 0;

    while (1) {
        char c = keyboard_getchar();

        if (c == 0) {
            /* 没有按键, 让出 CPU */
            __asm__ volatile("hlt");
            continue;
        }

        if (c == '\n') {
            buffer[pos] = '\0';
            terminal_putchar('\n');
            break;
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                terminal_putchar('\b');
            }
        } else if (c == 0x1B) {
            /* ESC: 清空输入 */
            while (pos > 0) {
                pos--;
                terminal_putchar('\b');
            }
        } else if (c >= ' ' && pos < max_len - 1) {
            buffer[pos++] = c;
            terminal_putchar(c);
        }
    }
}

char keyboard_getchar_nonblock(void)
{
    return keyboard_getchar();
}

int keyboard_poll(void)
{
    int k = keyboard_special_key;
    if (k) keyboard_special_key = 0;
    return k;
}

void keyboard_clear_special(void)
{
    keyboard_special_key = 0;
    ext_scancode = 0;
}