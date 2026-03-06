#include <stdint.h>

#define ATA_DATA        0x1F0
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7

#define ATA_STATUS_BSY  0x80
#define ATA_STATUS_DRQ  0x08
#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static void ata_wait(void) {
    while (inb(ATA_STATUS) & ATA_STATUS_BSY);
}

void ata_read(uint32_t lba, uint8_t count, void *buf) {
    uint16_t *ptr = (uint16_t *)buf;
    ata_wait();
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_CNT, count);
    outb(ATA_LBA_LOW,   (uint8_t)(lba));
    outb(ATA_LBA_MID,   (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH,  (uint8_t)(lba >> 16));
    outb(ATA_COMMAND,   ATA_CMD_READ);
    for (int s = 0; s < count; s++) {
        ata_wait();
        while (!(inb(ATA_STATUS) & ATA_STATUS_DRQ));
        for (int i = 0; i < 256; i++)
            ptr[s * 256 + i] = inw(ATA_DATA);
    }
}

void ata_write(uint32_t lba, uint8_t count, void *buf) {
    uint16_t *ptr = (uint16_t *)buf;
    ata_wait();
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_CNT, count);
    outb(ATA_LBA_LOW,   (uint8_t)(lba));
    outb(ATA_LBA_MID,   (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH,  (uint8_t)(lba >> 16));
    outb(ATA_COMMAND,   ATA_CMD_WRITE);
    for (int s = 0; s < count; s++) {
        ata_wait();
        while (!(inb(ATA_STATUS) & ATA_STATUS_DRQ));
        for (int i = 0; i < 256; i++)
            outw(ATA_DATA, ptr[s * 256 + i]);
        outb(ATA_COMMAND, 0xE7);
        ata_wait();
    }
}
