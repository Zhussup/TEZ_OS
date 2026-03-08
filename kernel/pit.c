#include <stdint.h>

#define PIT_CHANNEL0 0x40
#define PIT_CMD      0x43
#define PIT_HZ       100  /* 100 ticks/sec */
#define PIT_DIVISOR  (1193180 / PIT_HZ)

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

extern void pic_send_eoi(void);
extern void clock_update(void);

static volatile uint32_t ticks = 0;

void pit_handler(void) {
    ticks++;
    if (ticks % PIT_HZ == 0)
        clock_update();
    pic_send_eoi();
}

void pit_init(void) {
    outb(PIT_CMD, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(PIT_DIVISOR & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)(PIT_DIVISOR >> 8));
}

uint32_t pit_ticks(void) {
    return ticks;
}