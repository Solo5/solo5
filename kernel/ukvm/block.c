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

#include "kernel.h"

/*
 * TODO: To avoid changing both the Solo5 API and ukvm hypercall ABI at the
 * same time, this module currently performs some translation between the two.
 * This translation will be removed in a separate change which will align the
 * ukvm hypercall ABI to match the Solo5 API.
 */

static struct solo5_block_info block_info;

static void init_block_info(void)
{
    volatile struct ukvm_blkinfo bi;

    ukvm_do_hypercall(UKVM_HYPERCALL_BLKINFO, &bi);

    block_info.block_size = bi.sector_size;
    block_info.capacity = bi.num_sectors * bi.sector_size;
}

solo5_result_t solo5_block_write(solo5_off_t offset, const uint8_t *buf,
        size_t size)
{
    if (!block_info.capacity)
        init_block_info();

    /*
     * XXX: This does not check for writes ending past the end of the device,
     * current ukvm will return -1 (translated to SOLO5_R_EUNSPEC) below.
     */
    if ((offset % block_info.block_size != 0) ||
        (offset >= block_info.capacity) ||
        (size != block_info.block_size))
        return SOLO5_R_EINVAL;

    volatile struct ukvm_blkwrite wr;
    wr.sector = offset / block_info.block_size;
    wr.data = buf;
    wr.len = size;
    wr.ret = 0;

    ukvm_do_hypercall(UKVM_HYPERCALL_BLKWRITE, &wr);

    return (wr.ret == 0) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_block_read(solo5_off_t offset, uint8_t *buf, size_t size)
{
    if (!block_info.capacity)
        init_block_info();

    /*
     * XXX: This does not check for reads ending past the end of the device,
     * current ukvm will return -1 (translated to SOLO5_R_EUNSPEC) below.
     */
    if ((offset % block_info.block_size != 0) ||
        (offset >= block_info.capacity) ||
        (size != block_info.block_size))
        return SOLO5_R_EINVAL;

    volatile struct ukvm_blkread rd;
    rd.sector = offset / block_info.block_size;
    rd.data = buf;
    rd.len = size;
    rd.ret = 0;

    ukvm_do_hypercall(UKVM_HYPERCALL_BLKREAD, &rd);

    return (rd.ret == 0 && rd.len == size) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}

void solo5_block_info(struct solo5_block_info *info)
{
    if (!block_info.capacity)
        init_block_info();

    *info = block_info;
}
