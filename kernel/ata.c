#include <stdint.h>

// Primary ATA Ports
#define ATA_DATA        0x1F0
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7

#define ATA_STATUS_BSY  0x80  // Disk is busy
#define ATA_STATUS_DRQ  0x08  // Ready for data
#define ATA_CMD_READ    0x20  // Command: read

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

// Waiting until disk is ready
static void ata_wait(void) {
    while (inb(ATA_STATUS) & ATA_STATUS_BSY);
}

// Reading count sectors starts with lba into buffer buf
void ata_read(uint32_t lba, uint8_t count, void *buf) {
    uint16_t *ptr = (uint16_t *)buf;

    ata_wait();
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F)); // Master, LBA mode
    outb(ATA_SECTOR_CNT, count);
    outb(ATA_LBA_LOW,   (uint8_t)(lba));
    outb(ATA_LBA_MID,   (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH,  (uint8_t)(lba >> 16));
    outb(ATA_COMMAND,   ATA_CMD_READ);

    for (int s = 0; s < count; s++) {
        ata_wait();
        while (!(inb(ATA_STATUS) & ATA_STATUS_DRQ)); // Waiting DRQ
        for (int i = 0; i < 256; i++)                // 256 * 2 = 512 bytes
            ptr[s * 256 + i] = inw(ATA_DATA);
    }
}