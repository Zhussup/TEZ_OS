#include <stdint.h>

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

static int rtc_updating(void) {
    outb(CMOS_ADDR, 0x0A);
    return inb(CMOS_DATA) & 0x80;
}

typedef struct {
    uint8_t sec, min, hour;
    uint8_t day, month;
    uint16_t year;
} rtc_time_t;

void rtc_get(rtc_time_t *t) {
    while (rtc_updating());

    uint8_t sec   = cmos_read(0x00);
    uint8_t min   = cmos_read(0x02);
    uint8_t hour  = cmos_read(0x04);
    uint8_t day   = cmos_read(0x07);
    uint8_t month = cmos_read(0x08);
    uint8_t year  = cmos_read(0x09);

    uint8_t regb = cmos_read(0x0B);
    if (!(regb & 0x04)) {
        sec   = bcd_to_bin(sec);
        min   = bcd_to_bin(min);
        hour  = bcd_to_bin(hour);
        day   = bcd_to_bin(day);
        month = bcd_to_bin(month);
        year  = bcd_to_bin(year);
    }

    t->sec   = sec;
    t->min   = min;
    t->hour  = (hour + 5) % 24;  /* UTC+5 */
    t->day   = day;
    t->month = month;
    t->year  = 2000 + year;
}