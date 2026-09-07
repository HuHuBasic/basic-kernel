/* interrupt.c — IDT / PIC / 中断处理 */

#include "kernel.h"

struct idt_entry {
    uint16 base_lo;
    uint16 sel;
    uint8  always0;
    uint8  flags;
    uint16 base_hi;
} __attribute__((packed));

struct idt_ptr {
    uint16 limit;
    uint32 base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

#define IDT_FLAG_PRESENT  0x80
#define IDT_FLAG_RING0    0x00
#define IDT_FLAG_INT32    0x0E

void idt_set_gate(int num, uint32 base, uint16 sel, uint8 flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void idt_install(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32)&idt;
    for (int i = 0; i < 256; i++)
        idt_set_gate(i, 0, 0, 0);
    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFC);  /* IRQ0+IRQ1 */
    outb(0xA1, 0xFF);
}

/* 键盘 */
#define KEYBOARD_PORT 0x60
static const char kbd_us[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
    [0x3A ... 127] = 0
};

static const char kbd_us_shift[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',
    [0x3A ... 127] = 0
};

static int shift_pressed = 0, caps_lock = 0;

/* 键盘回调 — 由 shell 设置 */
static void (*kbd_callback)(char c) = NULL;
void kbd_set_callback(void (*cb)(char c)) { kbd_callback = cb; }

static void __attribute__((used)) irq_handler(uint32 irq) {
    if (irq == 0) {
        outb(0x20, 0x20);
    } else if (irq == 1) {
        uint8 sc = inb(KEYBOARD_PORT);
        if (sc == 0x2A || sc == 0x36) shift_pressed = 1;
        else if (sc == 0xAA || sc == 0xB6) shift_pressed = 0;
        else if (sc == 0x3A) caps_lock = !caps_lock;
        else if (sc < 128 && kbd_callback) {
            char c = (shift_pressed || caps_lock) ? kbd_us_shift[sc] : kbd_us[sc];
            if (c != 0) kbd_callback(c);
        }
        outb(0x20, 0x20);
    }
}

static void __attribute__((used)) exc_handler(uint32 num) {
    vga_set_color(LRED, BLACK);
    vga_write("\n*** EXCEPTION #");
    vga_dec(num);
    vga_write(" — System Halted ***\n");
    vga_set_color(LGRAY, BLACK);
    for (;;) __asm__ volatile ("hlt");
}

/* 中断存根 */
#define IRQ_STUB(n) \
    __asm__ volatile ( \
        ".global irq_stub_" #n "\n" \
        "irq_stub_" #n ":\n" \
        "    pushal\n" \
        "    push $" #n "\n" \
        "    call irq_handler\n" \
        "    addl $4, %esp\n" \
        "    popal\n" \
        "    iret\n" \
    )

#define EXC_STUB(n) \
    __asm__ volatile ( \
        ".global exc_stub_" #n "\n" \
        "exc_stub_" #n ":\n" \
        "    pushal\n" \
        "    push $" #n "\n" \
        "    call exc_handler\n" \
        "    addl $4, %esp\n" \
        "    popal\n" \
        "    iret\n" \
    )

static void install_irq_stubs(void) {
    IRQ_STUB(0);IRQ_STUB(1);IRQ_STUB(2);IRQ_STUB(3);IRQ_STUB(4);
    IRQ_STUB(5);IRQ_STUB(6);IRQ_STUB(7);IRQ_STUB(8);IRQ_STUB(9);
    IRQ_STUB(10);IRQ_STUB(11);IRQ_STUB(12);IRQ_STUB(13);IRQ_STUB(14);IRQ_STUB(15);
}

static void install_exc_stubs(void) {
    EXC_STUB(0);EXC_STUB(1);EXC_STUB(2);EXC_STUB(3);EXC_STUB(4);
    EXC_STUB(5);EXC_STUB(6);EXC_STUB(7);EXC_STUB(8);EXC_STUB(9);
    EXC_STUB(10);EXC_STUB(11);EXC_STUB(12);EXC_STUB(13);EXC_STUB(14);EXC_STUB(15);
    EXC_STUB(16);EXC_STUB(17);EXC_STUB(18);EXC_STUB(19);EXC_STUB(20);
    EXC_STUB(21);EXC_STUB(22);EXC_STUB(23);EXC_STUB(24);EXC_STUB(25);
    EXC_STUB(26);EXC_STUB(27);EXC_STUB(28);EXC_STUB(29);EXC_STUB(30);EXC_STUB(31);
}

extern void irq_stub_0(void);extern void irq_stub_1(void);extern void irq_stub_2(void);
extern void irq_stub_3(void);extern void irq_stub_4(void);extern void irq_stub_5(void);
extern void irq_stub_6(void);extern void irq_stub_7(void);extern void irq_stub_8(void);
extern void irq_stub_9(void);extern void irq_stub_10(void);extern void irq_stub_11(void);
extern void irq_stub_12(void);extern void irq_stub_13(void);extern void irq_stub_14(void);
extern void irq_stub_15(void);

extern void exc_stub_0(void);extern void exc_stub_1(void);extern void exc_stub_2(void);
extern void exc_stub_3(void);extern void exc_stub_4(void);extern void exc_stub_5(void);
extern void exc_stub_6(void);extern void exc_stub_7(void);extern void exc_stub_8(void);
extern void exc_stub_9(void);extern void exc_stub_10(void);extern void exc_stub_11(void);
extern void exc_stub_12(void);extern void exc_stub_13(void);extern void exc_stub_14(void);
extern void exc_stub_15(void);extern void exc_stub_16(void);extern void exc_stub_17(void);
extern void exc_stub_18(void);extern void exc_stub_19(void);extern void exc_stub_20(void);
extern void exc_stub_21(void);extern void exc_stub_22(void);extern void exc_stub_23(void);
extern void exc_stub_24(void);extern void exc_stub_25(void);extern void exc_stub_26(void);
extern void exc_stub_27(void);extern void exc_stub_28(void);extern void exc_stub_29(void);
extern void exc_stub_30(void);extern void exc_stub_31(void);

void interrupt_init(void) {
    install_irq_stubs();
    install_exc_stubs();
    idt_install();

    void *irq_s[] = {irq_stub_0,irq_stub_1,irq_stub_2,irq_stub_3,irq_stub_4,irq_stub_5,
        irq_stub_6,irq_stub_7,irq_stub_8,irq_stub_9,irq_stub_10,irq_stub_11,
        irq_stub_12,irq_stub_13,irq_stub_14,irq_stub_15};
    void *exc_s[] = {exc_stub_0,exc_stub_1,exc_stub_2,exc_stub_3,exc_stub_4,exc_stub_5,
        exc_stub_6,exc_stub_7,exc_stub_8,exc_stub_9,exc_stub_10,exc_stub_11,
        exc_stub_12,exc_stub_13,exc_stub_14,exc_stub_15,exc_stub_16,exc_stub_17,
        exc_stub_18,exc_stub_19,exc_stub_20,exc_stub_21,exc_stub_22,exc_stub_23,
        exc_stub_24,exc_stub_25,exc_stub_26,exc_stub_27,exc_stub_28,exc_stub_29,
        exc_stub_30,exc_stub_31};

    for (int i = 0; i < 32; i++)
        idt_set_gate(i, (uint32)exc_s[i], 0x08, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INT32);

    pic_remap();

    for (int i = 0; i < 16; i++)
        idt_set_gate(32 + i, (uint32)irq_s[i], 0x08, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INT32);

    __asm__ volatile ("sti");
    vga_write("  [OK] Interrupts enabled\n");
}