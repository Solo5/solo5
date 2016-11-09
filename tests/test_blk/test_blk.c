#include "solo5.h"

static size_t strlen(const char *s)
{
    size_t len = 0;

    while (*s++)
        len += 1;
    return len;
}

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

#define SECTOR_SIZE	512

static uint8_t sector_write[SECTOR_SIZE];
static uint8_t sector_read[SECTOR_SIZE];

int check_sector_write(uint64_t sector)
{
    int n = SECTOR_SIZE;
    int i;

    for (i = 0; i < SECTOR_SIZE; i++) {
        sector_write[i] = '0' + i % 10;
        sector_read[i] = 0;
    }

    solo5_blk_write_sync(sector, sector_write, SECTOR_SIZE);
    solo5_blk_read_sync(sector, sector_read, &n);

    if (n != SECTOR_SIZE)
        return 1;
    
    for (i = 0; i < SECTOR_SIZE; i++) {
        if (sector_read[i] != '0' + i % 10)
            /* Check failed */
            return 1;
    }

    return 0;
}

int solo5_app_main(char *cmdline __attribute__((unused)))
{
    uint64_t i;

    puts("\n**** Solo5 standalone test_blk ****\n\n");

    /* Write and read/check one tenth of the disk. */
    for (i = 0; i < solo5_blk_sectors(); i += 10) {
        if (check_sector_write(i))
            /* Check failed */
            return 1;
    }

    puts("SUCCESS\n");

    return 0;
}
