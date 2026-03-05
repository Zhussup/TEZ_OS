#include <stdint.h>

#define VGA_MEMORY  ((char*)0xB8000)
#define WHITE       0x0F
#define CYAN        0x0B
#define GREEN       0x0A

extern void idt_init(void);
extern void irq_install_handler(int irq, uint32_t handler);
extern void keyboard_isr(void);
extern int  keyboard_has_data(void);
extern char keyboard_getchar(void);

// --- VGA ---
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

void vga_print_color(const char *s, uint8_t color) {
    char *vga = VGA_MEMORY;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\n') { cursor = (cursor / 80 + 1) * 80; continue; }
        vga[cursor * 2]     = s[i];
        vga[cursor * 2 + 1] = color;
        cursor++;
    }
}

void vga_clear(void) {
    char *vga = VGA_MEMORY;
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        vga[i] = ' '; vga[i+1] = WHITE;
    }
    cursor = 0;
}

// --- fetchy indpired:) ---
void neofetch(void) {
    vga_print_color(" _______ _______ _______          _______ _______ \n", CYAN);
    vga_print_color("|__   __|  _____|___   /        |  ___  ||  _____|\n", CYAN);
    vga_print_color("   | |  | |___     / /          | |   | || |______\n", CYAN);
    vga_print_color("   | |  |  ___|   / /           | |   | || ____  |\n", CYAN);
    vga_print_color("   | |  | |___   / /__  _______ | |___| ||_____| |\n", CYAN);
    vga_print_color("   |_|  |_____|_/_____| |_____| |_______||_______|\n", CYAN);
    vga_print("\n");
    vga_print_color("  OS:      ", WHITE); vga_print_color("TEZ_OS\n",           GREEN);
    vga_print_color("  Arch:    ", WHITE); vga_print_color("x86 32-bit\n",       GREEN);
    vga_print_color("  Kernel:  ", WHITE); vga_print_color("JZA kernel\n",   GREEN);
    vga_print_color("  Boot:    ", WHITE); vga_print_color("custom bootloader\n", GREEN);
    vga_print_color("  FS:      ", WHITE); vga_print_color("FAT12\n",            GREEN);
    vga_print_color("  Display: ", WHITE); vga_print_color("VGA text 80x25\n",   GREEN);
    vga_print_color("  Input:   ", WHITE); vga_print_color("PS/2 keyboard\n",    GREEN);
    vga_print("\n");
}

// --- Kernel ---
void kernel_main(void) {
    vga_clear();
    idt_init();
    irq_install_handler(1, (uint32_t)keyboard_isr);
    __asm__ volatile ("sti");

    neofetch();

    vga_print_color("> ", CYAN);

    char line[128];
    int pos = 0;

    while (1) {
        if (keyboard_has_data()) {
            char c = keyboard_getchar();
            if (c == '\n') {
                line[pos] = '\0';
                vga_putchar('\n');
                vga_print_color("> ", CYAN);
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
