/*
 * Copyright (c) 2017 Contributors as noted in the AUTHORS file
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

#include "../kernel.h"

/* ukvm block interface */
int solo5_blk_write_sync(uint64_t sec __attribute__((unused)),
                         uint8_t *data __attribute__((unused)),
                         int n __attribute__((unused)))
{
    return -1;
}

int solo5_blk_read_sync(uint64_t sec __attribute__((unused)),
                        uint8_t *data __attribute__((unused)),
                        int *n __attribute__((unused)))
{
    return -1;
}

int solo5_blk_sector_size(void)
{
    return -1;
}

uint64_t solo5_blk_sectors(void)
{
    return 0;
}

int solo5_blk_rw(void)
{
    return -1;
}
