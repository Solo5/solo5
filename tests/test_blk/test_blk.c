/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a unikernel base layer.
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "solo5.h"
#include "../../kernel/lib.c"

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

#define SECTOR_SIZE	512

/* Space for 2 sectors for edge-case tests */
static uint8_t wbuf[SECTOR_SIZE * 2];
static uint8_t rbuf[SECTOR_SIZE * 2];

int check_sector_write(uint64_t sector)
{
    int rlen = SECTOR_SIZE;
    unsigned i;

    for (i = 0; i < SECTOR_SIZE; i++) {
        wbuf[i] = '0' + i % 10;
        rbuf[i] = 0;
    }

    if (solo5_blk_write_sync(sector, wbuf, SECTOR_SIZE) != 0)
        return 1;
    if (solo5_blk_read_sync(sector, rbuf, &rlen) != 0)
        return 1;

    if (rlen != SECTOR_SIZE)
        return 1;
    
    for (i = 0; i < SECTOR_SIZE; i++) {
        if (rbuf[i] != '0' + i % 10)
            /* Check failed */
            return 1;
    }

    return 0;
}

int solo5_app_main(const struct solo5_boot_info *bi __attribute__((unused)))
{
    size_t i, nsectors;
    int rlen;

    puts("\n**** Solo5 standalone test_blk ****\n\n");

    /*
     * Write and read/check one tenth of the disk.
     */
    nsectors = solo5_blk_sectors();
    for (i = 0; i <= nsectors; i += 10) {
        if (check_sector_write(i))
            /* Check failed */
            return 1;
    }

    /*
     * Check edge case: read/write of last sector on the device.
     */
    if (solo5_blk_write_sync(nsectors - 1, wbuf, SECTOR_SIZE) != 0)
        return 2;
    rlen = SECTOR_SIZE;
    if (solo5_blk_read_sync(nsectors - 1, rbuf, &rlen) != 0)
        return 3;
    if (rlen != SECTOR_SIZE)
        return 4;

    /*
     * Check edge cases: should not be able to read or write beyond end
     * of device.
     *
     * XXX Multi-sector block operations currently work only on ukvm, virtio
     * will always return -1 here.
     */
    if (solo5_blk_write_sync(nsectors - 1, wbuf, 2 * SECTOR_SIZE) != -1)
        return 5;
    rlen = 2 * SECTOR_SIZE;
    if (solo5_blk_read_sync(nsectors - 1, rbuf, &rlen) != -1)
        return 6;

    puts("SUCCESS\n");

    return SOLO5_EXIT_SUCCESS;
}
