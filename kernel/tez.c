#include <stdint.h>
#include "tez.h"

extern void vga_print(const char *s);
extern void vga_print_color(const char *s, uint8_t color);
extern void vga_putchar(char c);
extern int  keyboard_has_data(void);
extern char keyboard_getchar(void);
extern int  fat12_read(const char *name, void *buf, uint32_t buf_size);

#define WHITE  0x0F
#define CYAN   0x0B
#define GREEN  0x0A
#define RED    0x0C
#define YELLOW 0x0E

/* executable code buffer — placed in BSS, writable+executable in flat 32-bit */
static uint8_t tez_exec_buf[TEZ_MAX_CODE];

const char *tez_parse_error(void);

void tez_run(const char *src) {
    int tok_count = tez_lex(src);
    if (tok_count == 0) return;

    int root = tez_parse();
    if (root < 0) {
        vga_print_color("parse error: ", RED);
        vga_print(tez_parse_error());
        vga_putchar('\n');
        return;
    }

extern const char *tez_codegen_error(void);

    int sz = tez_codegen(root, tez_exec_buf, TEZ_MAX_CODE);
    if (sz < 0) {
        vga_print_color("codegen error: ", RED);
        vga_print(tez_codegen_error());
        vga_putchar('\n');
        return;
    }

    /* execute — flat 32-bit, no protection, just call the buffer */
    typedef void (*fn_t)(void);
    fn_t fn = (fn_t)tez_exec_buf;
    fn();
}

void tez_run_file(const char *fat_name) {
    static char src_buf[4096];
    int bytes = fat12_read(fat_name, src_buf, sizeof(src_buf)-1);
    if (bytes < 0) {
        vga_print_color("run: file not found\n", RED);
        return;
    }
    src_buf[bytes] = 0;
    tez_run(src_buf);
}

void tez_repl(void) {
    vga_print_color("tez 0.1  |  Ctrl+Q to quit\n", CYAN);
    vga_print_color("type tez code, end with Ctrl+Enter (or use 'run <file.tez>')\n\n", 0x08);

    static char line[512];
    int pos = 0;

    vga_print_color(">> ", CYAN);

    while (1) {
        if (!keyboard_has_data()) continue;
        char c = keyboard_getchar();

        if (c == 0x11) { vga_putchar('\n'); break; } /* Ctrl+Q */

        if (c == '\n') {
            line[pos] = 0;
            vga_putchar('\n');
            if (pos > 0) tez_run(line);
            pos = 0;
            vga_print_color(">> ", CYAN);
        } else if (c == '\b') {
            if (pos > 0) { pos--; vga_putchar('\b'); }
        } else if (pos < 511) {
            line[pos++] = c;
            vga_putchar(c);
        }
    }
}