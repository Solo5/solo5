#include "solo5.h"

#define SECTOR_SIZE	512

int check_sector_write(uint64_t offset)
{
    uint8_t sector_write[SECTOR_SIZE];
    uint8_t sector_read[SECTOR_SIZE];
    int n = SECTOR_SIZE;
    int i;

    for (i = 0; i < SECTOR_SIZE; i++)
        sector_write[i] = '0' + i % 10;

    solo5_blk_write_sync(offset, sector_write, SECTOR_SIZE);
    solo5_blk_read_sync(offset, sector_read, &n);

    if (n != SECTOR_SIZE)
        return 1;
    
    for (i = 0; i < SECTOR_SIZE; i++) {
        if (sector_read[i] != '0' + i % 10)
            /* Check failed */
            return 1;
    }

    return 0;
}

/* Returns 0 if the tests pass, 1 otherwise. */
int solo5_app_main(char *cmdline __attribute__((unused)))
{
    uint64_t i;

    /* Write and read/check one tenth of the disk. */
    for (i = 0; i < solo5_blk_sectors(); i += 10) {
        if (check_sector_write(i * SECTOR_SIZE))
            /* Check failed */
            return 1;
    }

    return 0;
}
