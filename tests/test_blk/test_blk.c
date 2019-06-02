/*
 * Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a sandboxed execution environment.
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
#include "../../bindings/lib.c"

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

bool check_one_block(solo5_handle_t h, solo5_off_t offset, size_t block_size)
{
    size_t i;
    uint8_t wbuf[block_size],
            rbuf[block_size];

    for (i = 0; i < block_size; i++) {
        wbuf[i] = '0' + i % 10;
        rbuf[i] = 0;
    }

    if (solo5_block_write(h, offset, wbuf, block_size) != SOLO5_R_OK)
        return false;
    if (solo5_block_read(h, offset, rbuf, block_size) != SOLO5_R_OK)
        return false;

    for (i = 0; i < block_size; i++) {
        if (rbuf[i] != '0' + i % 10)
            /* Check failed */
            return false;
    }

    return true;
}

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    puts("\n**** Solo5 standalone test_blk ****\n\n");

    solo5_handle_t h;
    struct solo5_block_info bi;
    if (solo5_block_acquire("storage", &h, &bi) != SOLO5_R_OK) {
        puts("Could not acquire 'storage' block device\n");
        return 99;
    }

    /*
     * Write and read/check one tenth of the disk.
     */
    for (solo5_off_t offset = 0; offset < bi.capacity;
            offset += (10 * bi.block_size)) {
        if (!check_one_block(h, offset, bi.block_size))
            /* Check failed */
            return 1;
    }

    uint8_t buf[bi.block_size * 2];

    /*
     * Check edge case: read/write of last sector on the device.
     */
    solo5_off_t last_block = bi.capacity - bi.block_size;
    if (solo5_block_write(h, last_block, buf, bi.block_size) != SOLO5_R_OK)
        return 2;
    if (solo5_block_read(h, last_block, buf, bi.block_size) != SOLO5_R_OK)
        return 3;

    /*
     * Check edge cases: should not be able to read or write beyond end
     * of device.
     * 
     * XXX: Current implementations may return either SOLO5_R_EINVAL or
     * SOLO5_R_EUNSPEC here, that is fine for now.
     */
    if (solo5_block_write(h, bi.capacity, buf, bi.block_size) == SOLO5_R_OK)
        return 4;
    if (solo5_block_read(h, bi.capacity, buf, bi.block_size) == SOLO5_R_OK)
        return 5;

    /*
     * Check invalid arguments: Should not be able to read or write less than
     * bi.block_size or more than bi.block_size.
     *
     * XXX: Current implementations may return either SOLO5_R_EINVAL or
     * SOLO5_R_EUNSPEC here, that is fine for now.
     */
    if (solo5_block_write(h, 0, buf, bi.block_size - 1) == SOLO5_R_OK)
        return 6;
    if (solo5_block_read(h, 0, buf, bi.block_size - 1) == SOLO5_R_OK)
        return 7;
    if (solo5_block_write(h, 0, buf, bi.block_size + 1) == SOLO5_R_OK)
        return 8;
    if (solo5_block_read(h, 0, buf, bi.block_size + 1) == SOLO5_R_OK)
        return 9;

    /*
     * Check invalid arguments: Should not be able to read or write at offsets
     * not aligned to bi.block_size.
     *
     * XXX: Current implementations may return either SOLO5_R_EINVAL or
     * SOLO5_R_EUNSPEC here, that is fine for now.
     */
    if (solo5_block_write(h, bi.block_size - 1, buf, bi.block_size)
            == SOLO5_R_OK)
        return 10;
    if (solo5_block_read(h, bi.block_size - 1, buf, bi.block_size)
            == SOLO5_R_OK)
        return 11;

    puts("SUCCESS\n");

    return SOLO5_EXIT_SUCCESS;
}
