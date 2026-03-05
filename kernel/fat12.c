#include <stdint.h>

extern void ata_read(uint32_t lba, uint8_t count, void *buf);

// Parameters FAT12 for floppy 1.44MB
#define BYTES_PER_SECTOR    512
#define SECTORS_PER_CLUSTER 1
#define RESERVED_SECTORS    1
#define FAT_COUNT           2
#define ROOT_ENTRIES        224
#define SECTORS_PER_FAT     9

// Disk offset
#define FAT_START    1                                    // sectors before FAT
#define ROOT_START   (1 + FAT_COUNT * SECTORS_PER_FAT)   // = 19
#define ROOT_SECTORS ((ROOT_ENTRIES * 32) / BYTES_PER_SECTOR) // = 14
#define DATA_START   (ROOT_START + ROOT_SECTORS)          // = 33

// Structure of record в Root Directory (32 байта)
typedef struct {
    char     name[8];       // Name of the file
    char     ext[3];        // extension
    uint8_t  attributes;
    uint8_t  reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t first_cluster; // First files cluster
    uint32_t file_size;     // Size in bytes
} __attribute__((packed)) fat12_entry_t;

static uint8_t  fat_table[SECTORS_PER_FAT * BYTES_PER_SECTOR];
static uint8_t  root_buf[ROOT_SECTORS * BYTES_PER_SECTOR];
static int      fat_loaded = 0;

// Download FAT and Root Directory from disk
static void fat12_load(void) {
    if (fat_loaded) return;
    ata_read(FAT_START, SECTORS_PER_FAT, fat_table);
    ata_read(ROOT_START, ROOT_SECTORS, root_buf);
    fat_loaded = 1;
}

// Get next cluster from FAT12
static uint16_t fat12_next_cluster(uint16_t cluster) {
    uint32_t offset = cluster + (cluster / 2); // 12 bit for record
    uint16_t val = *(uint16_t *)(fat_table + offset);
    if (cluster & 1)
        return val >> 4;
    else
        return val & 0x0FFF;
}

// Find file in Root Directory by name (Format "FILENAME EXT")
fat12_entry_t *fat12_find(const char *name) {
    fat12_load();
    fat12_entry_t *entries = (fat12_entry_t *)root_buf;

    for (int i = 0; i < ROOT_ENTRIES; i++) {
        if (entries[i].name[0] == 0x00) break;  // end of directory
        if (entries[i].name[0] == 0xE5) continue; // deleted file

        // comparing names (11 symbols: 8 name + 3 extension)
        int match = 1;
        for (int j = 0; j < 11; j++) {
            char a = (j < 8) ? entries[i].name[j] : entries[i].ext[j-8];
            if (a != name[j]) { match = 0; break; }
        }
        if (match) return &entries[i];
    }
    return 0; // Not Found
}

// Read file into buffer buf (max buf_size byte)
int fat12_read(const char *name, void *buf, uint32_t buf_size) {
    fat12_entry_t *entry = fat12_find(name);
    if (!entry) return -1;

    uint16_t cluster = entry->first_cluster;
    uint32_t bytes_read = 0;
    uint8_t  sector_buf[BYTES_PER_SECTOR];

    while (cluster < 0xFF8 && bytes_read < buf_size) {
        // Cluster to sector
        uint32_t lba = DATA_START + (cluster - 2) * SECTORS_PER_CLUSTER;
        ata_read(lba, 1, sector_buf);

        uint32_t to_copy = BYTES_PER_SECTOR;
        if (bytes_read + to_copy > buf_size)
            to_copy = buf_size - bytes_read;

        // Copy into buf
        uint8_t *dst = (uint8_t *)buf + bytes_read;
        for (uint32_t i = 0; i < to_copy; i++)
            dst[i] = sector_buf[i];

        bytes_read += to_copy;
        cluster = fat12_next_cluster(cluster);
    }

    return bytes_read;
}