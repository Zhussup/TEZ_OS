#include <stdint.h>

// PIC Ports
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// IDT's one records structure
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector  = sel;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
}

// reprogramming PIC — IRQ0 = INT32, IRQ1 = INT33...
void pic_remap(void) {
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);
    outb(PIC1_DATA,   0x20);
    outb(PIC2_DATA,   0x28);
    outb(PIC1_DATA,   0x04);
    outb(PIC2_DATA,   0x02);
    outb(PIC1_DATA,   0x01);
    outb(PIC2_DATA,   0x01);
    // :
    outb(PIC1_DATA,   0xFD); // allowin only IRQ1 (kboard)
    outb(PIC2_DATA,   0xFF); // slave PIC all forbidden
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;
    // all to zero
    for (int i = 0; i < 256; i++)
        idt_set_gate(i, 0, 0, 0);
    pic_remap();
    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

void irq_install_handler(int irq, uint32_t handler) {
    idt_set_gate(32 + irq, handler, 0x08, 0x8E);
}

void pic_send_eoi(void) {
    outb(PIC1_COMMAND, 0x20); // End of Interrupt
}
