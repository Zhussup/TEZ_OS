#include <stdint.h>

#define VGA_MEMORY  ((char*)0xB8000)
#define WHITE       0x0F
#define CYAN        0x0B
#define GREEN       0x0A
#define YELLOW      0x0E
#define RED         0x0C

extern void idt_init(void);
extern void irq_install_handler(int irq, uint32_t handler);
extern void keyboard_isr(void);
extern int  keyboard_has_data(void);
extern char keyboard_getchar(void);
extern int  fat12_read(const char *name, void *buf, uint32_t buf_size);
extern int  fat12_ls(void);
extern void tuze_open(const char *fat_name);

static int cursor = 0;

// history
#define HISTORY_SIZE 16
static char history[HISTORY_SIZE][128];
static int  history_count = 0;
static int  history_idx   = -1;



static void vga_scroll(void) {
    char *vga = VGA_MEMORY;
    // move one line up
    for (int i = 0; i < 80 * 24 * 2; i++)
        vga[i] = vga[i + 80 * 2];
    // clearing last line
    for (int i = 80 * 24 * 2; i < 80 * 25 * 2; i += 2) {
        vga[i] = ' '; vga[i+1] = WHITE;
    }
    cursor -= 80;
}

void vga_putchar(char c) {
    char *vga = VGA_MEMORY;
    if (c == '\n') {
        cursor = (cursor / 80 + 1) * 80;
        if (cursor >= 80 * 25) vga_scroll();
        return;
    }
    if (c == '\b' && cursor > 0) {
        cursor--;
        vga[cursor * 2] = ' '; vga[cursor * 2 + 1] = WHITE;
        return;
    }
    vga[cursor * 2] = c; vga[cursor * 2 + 1] = WHITE;
    cursor++;
    if (cursor >= 80 * 25) vga_scroll();
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

static int str_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) { if (a[i] != b[i]) return 0; i++; }
    return a[i] == b[i];
}

static int str_starts(const char *a, const char *b) {
    int i = 0;
    while (b[i]) { if (a[i] != b[i]) return 0; i++; }
    return 1;
}

static void to_fat12_name(const char *src, char *dst) {
    int i, j;
    for (i = 0; i < 11; i++) dst[i] = ' ';
    dst[11] = 0;
    i = 0; j = 0;
    while (src[i] && src[i] != '.' && j < 8) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        dst[j++] = c;
        i++;
    }
    while (src[i] && src[i] != '.') i++;
    if (src[i] == '.') {
        i++; j = 8;
        while (src[i] && j < 11) {
            char c = src[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            dst[j++] = c;
            i++;
        }
    }
}

void neofetch(void) {
    vga_print_color(" _______ _______ _______          _______ _______ \n", CYAN);
    vga_print_color("|__   __|  _____|___   /        |  ___  ||  _____|\n", CYAN);
    vga_print_color("   | |  | |___     / /          | |   | || |______\n", CYAN);
    vga_print_color("   | |  |  ___|   / /           | |   | || ____  |\n", CYAN);
    vga_print_color("   | |  | |___   / /__  _______ | |___| ||_____| |\n", CYAN);
    vga_print_color("   |_|  |_____|_/_____| |_____| |_______||_______|\n", CYAN);
    vga_print("\n");
    vga_print_color("  OS:      ", WHITE); vga_print_color("TEZ_OS\n",            GREEN);
    vga_print_color("  Arch:    ", WHITE); vga_print_color("x86 32-bit\n",        GREEN);
    vga_print_color("  Kernel:  ", WHITE); vga_print_color("JZA kernel\n",        GREEN);
    vga_print_color("  Boot:    ", WHITE); vga_print_color("custom bootloader\n", GREEN);
    vga_print_color("  FS:      ", WHITE); vga_print_color("FAT12\n",             GREEN);
    vga_print_color("  Display: ", WHITE); vga_print_color("VGA text 80x25\n",    GREEN);
    vga_print_color("  Input:   ", WHITE); vga_print_color("PS/2 keyboard\n",     GREEN);
    vga_print("\n");
}

void cmd_hlp(void) {
    vga_print_color("Available commands:\n", YELLOW);
    vga_print_color("  hlp       ", CYAN); vga_print("- show this help\n");
    vga_print_color("  clr       ", CYAN); vga_print("- clear the screen\n");
    vga_print_color("  sinf      ", CYAN); vga_print("- show system info\n");
    vga_print_color("  room      ", CYAN); vga_print("- list files on disk\n");
    vga_print_color("  show <f>  ", CYAN); vga_print("- print file contents\n");
    vga_print_color("  tuze <f>  ", CYAN); vga_print("- edit file\n");
}

void cmd_show(const char *filename) {
    if (!filename || !filename[0]) {
        vga_print_color("Usage: show <filename>\n", RED);
        return;
    }
    char fat_name[12];
    to_fat12_name(filename, fat_name);
    static char file_buf[4096];
    int bytes = fat12_read(fat_name, file_buf, sizeof(file_buf) - 1);
    if (bytes < 0) {
        vga_print_color("show: not found: ", RED);
        vga_print(filename);
        vga_putchar('\n');
        return;
    }
    for (int i = 0; i < bytes; i++) {
        char c = file_buf[i];
        if (c >= 0x20 || c == '\n' || c == '\r' || c == '\t')
            vga_putchar(c);
    }
    if (bytes > 0 && file_buf[bytes-1] != '\n') vga_putchar('\n');
}

void shell_exec(const char *line) {
    if (!line[0]) return;

    // HISTORY
    if (line[0]) {
        int slot = history_count % HISTORY_SIZE;
        int i = 0;
        while (line[i]) { history[slot][i] = line[i]; i++; }
        history[slot][i] = 0;
        history_count++;
    }
    history_idx = -1;

    history_idx = -1;

    if      (str_eq(line, "hlp"))        cmd_hlp();
    else if (str_eq(line, "clr"))        vga_clear();
    else if (str_eq(line, "sinf"))       neofetch();
    else if (str_eq(line, "room"))       fat12_ls();
    else if (str_starts(line, "show "))  cmd_show(line + 5);
    else if (str_starts(line, "tuze ")) {
        char fat_name[12];
        to_fat12_name(line + 5, fat_name);
        vga_clear();
        tuze_open(fat_name);
        vga_clear();
        vga_print_color("type 'hlp' for commands\n\n", YELLOW);
        vga_print_color("> ", CYAN);
        return;
    }
    else {
        vga_print_color("unknown: ", RED);
        vga_print(line); vga_putchar('\n');
        vga_print_color("try 'hlp'\n", YELLOW);
    }
}

void kernel_main(void) {
    vga_clear();
    idt_init();
    irq_install_handler(1, (uint32_t)keyboard_isr);
    __asm__ volatile ("sti");

    neofetch();
    vga_print_color("type 'hlp' for commands\n\n", YELLOW);
    vga_print_color("> ", CYAN);

    char line[128];
    int pos = 0;

    while (1) {
        if (keyboard_has_data()) {
            char c = keyboard_getchar();
            if (c == '\n') {
                line[pos] = '\0';
                vga_putchar('\n');
                history_idx = -1;
                shell_exec(line);
                pos = 0;
                vga_print_color("> ", CYAN);
            } else if (c == '\b') {
                if (pos > 0) { pos--; vga_putchar('\b'); }
            } else if ((uint8_t)c == 0x80) { // KEY_UP
                if (history_count > 0) {
                    if (history_idx == -1)
                        history_idx = (history_count - 1) % HISTORY_SIZE;
                    else if (history_idx != (history_count > HISTORY_SIZE ?
                             (history_count % HISTORY_SIZE) : 0))
                        history_idx = (history_idx - 1 + HISTORY_SIZE) % HISTORY_SIZE;
                    // стираем текущую строку
                    while (pos > 0) { pos--; vga_putchar('\b'); }
                    // вставляем из истории
                    int i = 0;
                    while (history[history_idx][i]) {
                        line[pos++] = history[history_idx][i];
                        vga_putchar(history[history_idx][i]);
                        i++;
                    }
                }
            } else if ((uint8_t)c == 0x81) { // KEY_DOWN
                while (pos > 0) { pos--; vga_putchar('\b'); }
                if (history_idx != -1) {
                    history_idx = (history_idx + 1) % HISTORY_SIZE;
                    if (history_idx == history_count % HISTORY_SIZE) {
                        history_idx = -1;
                    } else {
                        int i = 0;
                        while (history[history_idx][i]) {
                            line[pos++] = history[history_idx][i];
                            vga_putchar(history[history_idx][i]);
                            i++;
                        }
                    }
                }
            } else if (pos < 127) {
                line[pos++] = c;
                vga_putchar(c);
            }
        }
    }
}
