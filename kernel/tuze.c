#include <stdint.h>

#define VGA_MEMORY  ((char*)0xB8000)
#define WHITE       0x0F
#define CYAN        0x0B
#define GREEN       0x0A
#define YELLOW      0x0E
#define RED         0x0C
#define BLACK_ON_WHITE 0x70
#define BLACK_ON_CYAN  0x30

#define COLS 80
#define ROWS 23  // 23 lines of text and + 1 status bar
#define MAX_SIZE 4096

#define KEY_UP    0x80
#define KEY_DOWN  0x81
#define KEY_LEFT  0x82
#define KEY_RIGHT 0x83

extern int  keyboard_has_data(void);
extern char keyboard_getchar(void);
extern void vga_print(const char *s);
extern void vga_print_color(const char *s, uint8_t color);
extern void vga_putchar(char c);
extern int  fat12_read(const char *name, void *buf, uint32_t buf_size);
extern int  fat12_write(const char *name, void *buf, uint32_t size);

static char  tbuf[MAX_SIZE];  // file's txt
static int   tlen = 0;        // text's length
static int   cx = 0, cy = 0; // cursor (column, raw)
static int   scroll = 0;      // first visible line
static char  tfilename[12];   // FAT12 file's name
static int   modified = 0;

// VGA straight access
static void vga_set(int row, int col, char c, uint8_t color) {
    char *vga = (char *)VGA_MEMORY;
    int idx = (row * COLS + col) * 2;
    vga[idx]     = c;
    vga[idx + 1] = color;
}

static void vga_fill_row(int row, char c, uint8_t color) {
    for (int i = 0; i < COLS; i++)
        vga_set(row, i, c, color);
}

// work w lines in puf-puf-buffer
// find the n'th line beginning
static int line_start(int n) {
    int pos = 0, line = 0;
    while (pos < tlen && line < n) {
        if (tbuf[pos] == '\n') line++;
        pos++;
    }
    return pos;
}

// n's line's len (without \n)
static int line_len(int n) {
    int pos = line_start(n);
    int len = 0;
    while (pos + len < tlen && tbuf[pos + len] != '\n')
        len++;
    return len;
}

// amount of lines  in puf-puf-buffer
static int line_count(void) {
    int count = 1;
    for (int i = 0; i < tlen; i++)
        if (tbuf[i] == '\n') count++;
    return count;
}

// DRAWING
static void tuze_draw(void) {
    int lines = line_count();

    for (int r = 0; r < ROWS; r++) {
        int line = scroll + r;
        vga_fill_row(r, ' ', WHITE);
        if (line >= lines) continue;

        int pos = line_start(line);
        int col = 0;
        while (pos < tlen && tbuf[pos] != '\n' && col < COLS) {
            vga_set(r, col, tbuf[pos], WHITE);
            col++;
            pos++;
        }
    }

    // statusbar
    vga_fill_row(ROWS, ' ', BLACK_ON_CYAN);
    // file's name
    int si = 0;
    while (tfilename[si] && si < 11) {
        vga_set(ROWS, si, tfilename[si], BLACK_ON_CYAN);
        si++;
    }
    // hints
    const char *hint = "  Ctrl+S save  Ctrl+Q quit";
    for (int i = 0; hint[i] && si + i < COLS; i++)
        vga_set(ROWS, si + i, hint[i], BLACK_ON_CYAN);
    if (modified)
        vga_set(ROWS, COLS - 1, '*', BLACK_ON_CYAN);

    // status bar (24'th line)
    vga_fill_row(24, ' ', BLACK_ON_WHITE);
    // cursor's pos
    char pos_str[16];
    // empty itoa
    int tmp = cy + 1, pi = 0;
    char tmp2[8]; int ti = 0;
    while (tmp > 0) { tmp2[ti++] = '0' + (tmp % 10); tmp /= 10; }
    pos_str[pi++] = 'L'; pos_str[pi++] = ':';
    for (int i = ti - 1; i >= 0; i--) pos_str[pi++] = tmp2[i];
    pos_str[pi++] = ' '; pos_str[pi++] = 'C'; pos_str[pi++] = ':';
    tmp = cx + 1; ti = 0;
    while (tmp > 0) { tmp2[ti++] = '0' + (tmp % 10); tmp /= 10; }
    for (int i = ti - 1; i >= 0; i--) pos_str[pi++] = tmp2[i];
    pos_str[pi] = 0;
    for (int i = 0; pos_str[i]; i++)
        vga_set(24, i, pos_str[i], BLACK_ON_WHITE);

    // cursor
    int cur_row = cy - scroll;
    if (cur_row >= 0 && cur_row < ROWS)
        vga_set(cur_row, cx, tbuf[line_start(cy) + cx], 0x70);
}

// cursur's pos in puf-puf-buffer
static int cursor_pos(void) {
    return line_start(cy) + cx;
}

// insert char
static void insert_char(char c) {
    if (tlen >= MAX_SIZE - 1) return;
    int pos = cursor_pos();
    for (int i = tlen; i > pos; i--)
        tbuf[i] = tbuf[i - 1];
    tbuf[pos] = c;
    tlen++;
    modified = 1;
    if (c == '\n') {
        cy++;
        cx = 0;
    } else {
        cx++;
    }
}

// (backspace)
static void delete_char(void) {
    int pos = cursor_pos();
    if (pos == 0) return;
    for (int i = pos - 1; i < tlen - 1; i++)
        tbuf[i] = tbuf[i + 1];
    tlen--;
    modified = 1;
    // moving back
    if (cx > 0) {
        cx--;
    } else if (cy > 0) {
        cy--;
        cx = line_len(cy);
    }
}

// SAVING!!
static void tuze_save(void) {
    fat12_write(tfilename, tbuf, tlen);
    modified = 0;
}

// main loop of tuze
void tuze_open(const char *fat_name) {
    // copying name
    for (int i = 0; i < 11; i++) tfilename[i] = fat_name[i];
    tfilename[11] = 0;

    // reading if exists
    tlen = fat12_read(fat_name, tbuf, MAX_SIZE - 1);
    if (tlen < 0) tlen = 0;
    tbuf[tlen] = 0;

    cx = 0; cy = 0; scroll = 0; modified = 0;

    tuze_draw();

    while (1) {
        if (!keyboard_has_data()) continue;
        char c = keyboard_getchar();

        // Ctrl+S = 0x13, Ctrl+Q = 0x11
        if (c == 0x13) {
            tuze_save();
            tuze_draw();
            continue;
        }
        if (c == 0x11) {
            break; // quit
        }

        int lines = line_count();

        if ((uint8_t)c == KEY_UP) {
            if (cy > 0) {
                cy--;
                int ll = line_len(cy);
                if (cx > ll) cx = ll;
                if (cy < scroll) scroll--;
            }
        } else if ((uint8_t)c == KEY_DOWN) {
            if (cy < lines - 1) {
                cy++;
                int ll = line_len(cy);
                if (cx > ll) cx = ll;
                if (cy - scroll >= ROWS) scroll++;
            }
        } else if ((uint8_t)c == KEY_LEFT) {
            if (cx > 0) cx--;
            else if (cy > 0) { cy--; cx = line_len(cy); }
        } else if ((uint8_t)c == KEY_RIGHT) {
            int ll = line_len(cy);
            if (cx < ll) cx++;
            else if (cy < lines - 1) { cy++; cx = 0; }
        } else if (c == '\b') {
            delete_char();
            if (cy < scroll) scroll = cy;
        } else if (c == '\n') {
            insert_char('\n');
            if (cy - scroll >= ROWS) scroll++;
        } else if (c >= 0x20 && (uint8_t)c < 0x80) {
            insert_char(c);
        }

        tuze_draw();
    }
}
