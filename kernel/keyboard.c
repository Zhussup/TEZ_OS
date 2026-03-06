#include <stdint.h>
#define KEYBOARD_PORT 0x60
#define KEY_UP    0x80
#define KEY_DOWN  0x81
#define KEY_LEFT  0x82
#define KEY_RIGHT 0x83
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
extern void pic_send_eoi(void);
static const char sc_normal[] = {
    0, 0x1B, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' '
};
static const char sc_shift[] = {
    0, 0x1B, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' '
};
#define BUF_SIZE 256
static char buf[BUF_SIZE];
static int  head = 0, tail = 0;
static int  shift = 0, caps = 0, extended = 0;
static void buf_push(char c) {
    int next = (head + 1) % BUF_SIZE;
    if (next != tail) { buf[head] = c; head = next; }
}
void keyboard_handler(void) {
    uint8_t sc = inb(KEYBOARD_PORT);
    if (sc == 0xE0) {
        extended = 1;
        pic_send_eoi();
        return;
    }
    if (sc & 0x80) {
        uint8_t r = sc & ~0x80;
        if (r == 0x2A || r == 0x36) shift = 0;
        extended = 0;
    } else {
        if (extended) {
            switch (sc) {
                case 0x48: buf_push(KEY_UP);    break;
                case 0x50: buf_push(KEY_DOWN);  break;
                case 0x4B: buf_push(KEY_LEFT);  break;
                case 0x4D: buf_push(KEY_RIGHT); break;
            }
            extended = 0;
        } else {
            if (sc == 0x2A || sc == 0x36) { shift = 1; }
            else if (sc == 0x3A) { caps = !caps; }
            else if (sc < sizeof(sc_normal)) {
                char c = (shift ^ caps) ? sc_shift[sc] : sc_normal[sc];
                if (c) buf_push(c);
            }
        }
    }
    pic_send_eoi();
}
int keyboard_has_data(void) { return head != tail; }
char keyboard_getchar(void) {
    if (!keyboard_has_data()) return 0;
    char c = buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    return c;
}
