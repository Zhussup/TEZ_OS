#include <stdint.h>

#define VGA_MEMORY  ((char*)0xB8000)
#define WHITE       0x0F

extern void idt_init(void);
extern void irq_install_handler(int irq, uint32_t handler);
extern void keyboard_isr(void);
extern int  keyboard_has_data(void);
extern char keyboard_getchar(void);

static int cursor = 0;

void vga_putchar(char c) {
    char *vga = VGA_MEMORY;
    if (c == '\n') { cursor = (cursor / 80 + 1) * 80; return; }
    if (c == '\b' && cursor > 0) {
        cursor--;
        vga[cursor * 2] = ' '; vga[cursor * 2 + 1] = WHITE;
        return;
    }
    vga[cursor * 2] = c; vga[cursor * 2 + 1] = WHITE;
    cursor++;
}

void vga_print(const char *s) {
    for (int i = 0; s[i]; i++) vga_putchar(s[i]);
}

void vga_clear(void) {
    char *vga = VGA_MEMORY;
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        vga[i] = ' '; vga[i+1] = WHITE;
    }
    cursor = 0;
}

void kernel_main(void) {
    vga_clear();
    vga_print("=== MyOS ===\n");
    vga_print("Initializing IDT...\n");
    idt_init();
    irq_install_handler(1, (uint32_t)keyboard_isr);
    __asm__ volatile ("sti");
    vga_print("Keyboard ready!\n> ");

    char line[128];
    int pos = 0;
    while (1) {
        if (keyboard_has_data()) {
            char c = keyboard_getchar();
            if (c == '\n') {
                line[pos] = '\0';
                vga_putchar('\n');
                vga_print("You typed: ");
                vga_print(line);
                vga_putchar('\n');
                vga_print("> ");
                pos = 0;
            } else if (c == '\b') {
                if (pos > 0) { pos--; vga_putchar('\b'); }
            } else if (pos < 127) {
                line[pos++] = c;
                vga_putchar(c);
            }
        }
    }
}
