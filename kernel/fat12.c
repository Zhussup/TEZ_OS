#include <stdint.h>

extern void ata_read(uint32_t lba, uint8_t count, void *buf);
extern void vga_print(const char *s);
extern void vga_print_color(const char *s, uint8_t color);
extern void vga_putchar(char c);

#define CYAN   0x0B
#define WHITE  0x0F
#define GREEN  0x0A
#define YELLOW 0x0E

#define BYTES_PER_SECTOR    512
#define SECTORS_PER_CLUSTER 1
#define RESERVED_SECTORS    1
#define FAT_COUNT           2
#define ROOT_ENTRIES        224
#define SECTORS_PER_FAT     9

#define FAT_START    1
#define ROOT_START   (1 + FAT_COUNT * SECTORS_PER_FAT)
#define ROOT_SECTORS ((ROOT_ENTRIES * 32) / BYTES_PER_SECTOR)
#define DATA_START   (ROOT_START + ROOT_SECTORS)

typedef struct {
    char     name[8];
    char     ext[3];
    uint8_t  attributes;
    uint8_t  reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t first_cluster;
    uint32_t file_size;
} __attribute__((packed)) fat12_entry_t;

static uint8_t fat_table[SECTORS_PER_FAT * BYTES_PER_SECTOR];
static uint8_t root_buf[ROOT_SECTORS * BYTES_PER_SECTOR];
static int     fat_loaded = 0;

static void fat12_load(void) {
    if (fat_loaded) return;
    ata_read(FAT_START, SECTORS_PER_FAT, fat_table);
    ata_read(ROOT_START, ROOT_SECTORS, root_buf);
    fat_loaded = 1;
}

static uint16_t fat12_next_cluster(uint16_t cluster) {
    uint32_t offset = cluster + (cluster / 2);
    uint16_t val = *(uint16_t *)(fat_table + offset);
    if (cluster & 1)
        return val >> 4;
    else
        return val & 0x0FFF;
}

fat12_entry_t *fat12_find(const char *name) {
    fat12_load();
    fat12_entry_t *entries = (fat12_entry_t *)root_buf;
    for (int i = 0; i < ROOT_ENTRIES; i++) {
        if (entries[i].name[0] == 0x00) break;
        if (entries[i].name[0] == 0xE5) continue;
        int match = 1;
        for (int j = 0; j < 11; j++) {
            char a = (j < 8) ? entries[i].name[j] : entries[i].ext[j-8];
            if (a != name[j]) { match = 0; break; }
        }
        if (match) return &entries[i];
    }
    return 0;
}

int fat12_read(const char *name, void *buf, uint32_t buf_size) {
    fat12_entry_t *entry = fat12_find(name);
    if (!entry) return -1;
    // пустой файл — кластеров нет
    if (entry->file_size == 0 || entry->first_cluster < 2) return 0;
    uint16_t cluster = entry->first_cluster;
    uint32_t bytes_read = 0;
    uint8_t  sector_buf[BYTES_PER_SECTOR];
    while (cluster >= 2 && cluster < 0xFF8 && bytes_read < buf_size) {
        uint32_t lba = DATA_START + (cluster - 2) * SECTORS_PER_CLUSTER;
        ata_read(lba, 1, sector_buf);
        uint32_t to_copy = BYTES_PER_SECTOR;
        if (bytes_read + to_copy > buf_size)
            to_copy = buf_size - bytes_read;
        uint8_t *dst = (uint8_t *)buf + bytes_read;
        for (uint32_t i = 0; i < to_copy; i++)
            dst[i] = sector_buf[i];
        bytes_read += to_copy;
        cluster = fat12_next_cluster(cluster);
    }
    if (bytes_read > entry->file_size)
        bytes_read = entry->file_size;
    return bytes_read;
}

static void print_uint32(uint32_t val) {
    if (val == 0) { vga_putchar('0'); return; }
    char tmp[12]; int i = 0;
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    for (int j = i - 1; j >= 0; j--) vga_putchar(tmp[j]);
}

int fat12_ls(void) {
    fat12_load();
    fat12_entry_t *entries = (fat12_entry_t *)root_buf;

    vga_print_color("Name           Size\n", YELLOW);
    vga_print_color("------------   ----\n", WHITE);

    int count = 0;
    for (int i = 0; i < ROOT_ENTRIES; i++) {
        uint8_t first = (uint8_t)entries[i].name[0];
        if (first == 0x00) break;
        if (first == 0xE5) continue;
        if (entries[i].attributes == 0x0F) continue; // LFN
        if (entries[i].attributes & 0x08) continue;  // volume label

        uint8_t color = (entries[i].attributes & 0x10) ? CYAN : GREEN;

        for (int j = 0; j < 8; j++) vga_putchar(entries[i].name[j]);
        vga_putchar('.');
        for (int j = 0; j < 3; j++) vga_putchar(entries[i].ext[j]);
        vga_print("   ");

        if (entries[i].attributes & 0x10)
            vga_print_color("<DIR>\n", CYAN);
        else {
            print_uint32(entries[i].file_size);
            vga_putchar('\n');
        }
        count++;
    }

    vga_putchar('\n');
    print_uint32(count);
    vga_print(" file(s)\n");
    return count;
}

extern void ata_write(uint32_t lba, uint8_t count, void *buf);

// Записать значение в FAT таблицу (12-bit)
static void fat12_set(uint16_t cluster, uint16_t val) {
    uint32_t offset = cluster + (cluster / 2);
    if (cluster & 1) {
        fat_table[offset]     = (fat_table[offset] & 0x0F) | ((val << 4) & 0xF0);
        fat_table[offset + 1] = (val >> 4) & 0xFF;
    } else {
        fat_table[offset]     = val & 0xFF;
        fat_table[offset + 1] = (fat_table[offset + 1] & 0xF0) | ((val >> 8) & 0x0F);
    }
}

// Найти свободный кластер
static uint16_t fat12_alloc(void) {
    for (uint16_t c = 2; c < 2849; c++) {
        if (fat12_next_cluster(c) == 0x000)
            return c;
    }
    return 0; // нет места
}

// Записать FAT таблицу на диск (оба экземпляра)
static void fat12_flush_fat(void) {
    ata_write(FAT_START,                  SECTORS_PER_FAT, fat_table);
    ata_write(FAT_START + SECTORS_PER_FAT, SECTORS_PER_FAT, fat_table);
}

// Записать root directory на диск
static void fat12_flush_root(void) {
    ata_write(ROOT_START, ROOT_SECTORS, root_buf);
}

int fat12_delete(const char *name) {
    fat12_load();
    fat12_entry_t *entries = (fat12_entry_t *)root_buf;

    for (int i = 0; i < ROOT_ENTRIES; i++) {
        uint8_t first = (uint8_t)entries[i].name[0];
        if (first == 0x00) break;
        if (first == 0xE5) continue;

        int match = 1;
        for (int j = 0; j < 11; j++) {
            char a = (j < 8) ? entries[i].name[j] : entries[i].ext[j-8];
            if (a != name[j]) { match = 0; break; }
        }
        if (!match) continue;

        // освобождаем цепочку кластеров
        uint16_t c = entries[i].first_cluster;
        while (c >= 0x002 && c < 0xFF8) {
            uint16_t next = fat12_next_cluster(c);
            fat12_set(c, 0x000);
            c = next;
        }

        // помечаем запись как удалённую
        entries[i].name[0] = (char)0xE5;

        fat12_flush_fat();
        fat12_flush_root();
        return 0;
    }
    return -1; // не найден
}

int fat12_write(const char *name, void *buf, uint32_t size) {
    fat12_load();
    fat12_entry_t *entries = (fat12_entry_t *)root_buf;

    // ищем существующую запись или свободную
    fat12_entry_t *entry = fat12_find(name);
    if (!entry) {
        // ищем свободный слот в root
        for (int i = 0; i < ROOT_ENTRIES; i++) {
            uint8_t first = (uint8_t)entries[i].name[0];
            if (first == 0x00 || first == 0xE5) {
                entry = &entries[i];
                // заполняем имя
                for (int j = 0; j < 8; j++) entry->name[j] = name[j];
                for (int j = 0; j < 3; j++) entry->ext[j]  = name[8 + j];
                entry->attributes = 0x20;
                for (int j = 0; j < 10; j++) entry->reserved[j] = 0;
                entry->time = 0;
                entry->date = 0;
                entry->first_cluster = 0;
                entry->file_size = 0;
                break;
            }
        }
        if (!entry) return -1; // root directory полный
    } else {
        // освобождаем старые кластеры
        uint16_t c = entry->first_cluster;
        while (c >= 0x002 && c < 0xFF8) {
            uint16_t next = fat12_next_cluster(c);
            fat12_set(c, 0x000);
            c = next;
        }
        entry->first_cluster = 0;
    }

    // записываем данные по кластерам
    uint8_t sector_buf[BYTES_PER_SECTOR];
    uint32_t written = 0;
    uint16_t prev = 0;

    while (written < size) {
        uint16_t cluster = fat12_alloc();
        if (!cluster) return -1; // диск полон

        fat12_set(cluster, 0xFFF); // временно EOF

        if (prev)
            fat12_set(prev, cluster);
        else
            entry->first_cluster = cluster;

        // копируем 512 байт в sector_buf
        for (int i = 0; i < BYTES_PER_SECTOR; i++) {
            if (written + i < size)
                sector_buf[i] = ((uint8_t *)buf)[written + i];
            else
                sector_buf[i] = 0;
        }

        uint32_t lba = DATA_START + (cluster - 2) * SECTORS_PER_CLUSTER;
        ata_write(lba, 1, sector_buf);

        written += BYTES_PER_SECTOR;
        prev = cluster;
    }

    entry->file_size = size;

    // если size == 0 — файл пустой, кластеров нет, first_cluster уже 0
    if (size == 0) {
        fat12_flush_fat();
        fat12_flush_root();
        return 0;
    }

    fat12_flush_fat();
    fat12_flush_root();

    return size;
}