/*
 * Copyright (c) 2015-2018 Contributors as noted in the AUTHORS file
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

#include "bindings.h"

/*
 * TODO: To avoid changing both the Solo5 API and hvt hypercall ABI at the
 * same time, this module currently performs some translation between the two.
 * This translation will be removed in a separate change which will align the
 * hvt hypercall ABI to match the Solo5 API.
 */

static struct solo5_block_info block_info;

static void init_block_info(void)
{
    volatile struct hvt_blkinfo bi;

    hvt_do_hypercall(HVT_HYPERCALL_BLKINFO, &bi);

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
     * current hvt will return -1 (translated to SOLO5_R_EUNSPEC) below.
     */
    if ((offset % block_info.block_size != 0) ||
        (offset >= block_info.capacity) ||
        (size != block_info.block_size))
        return SOLO5_R_EINVAL;

    volatile struct hvt_blkwrite wr;
    wr.sector = offset / block_info.block_size;
    wr.data = buf;
    wr.len = size;
    wr.ret = 0;

    hvt_do_hypercall(HVT_HYPERCALL_BLKWRITE, &wr);

    return (wr.ret == 0) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_block_read(solo5_off_t offset, uint8_t *buf, size_t size)
{
    if (!block_info.capacity)
        init_block_info();

    /*
     * XXX: This does not check for reads ending past the end of the device,
     * current hvt will return -1 (translated to SOLO5_R_EUNSPEC) below.
     */
    if ((offset % block_info.block_size != 0) ||
        (offset >= block_info.capacity) ||
        (size != block_info.block_size))
        return SOLO5_R_EINVAL;

    volatile struct hvt_blkread rd;
    rd.sector = offset / block_info.block_size;
    rd.data = buf;
    rd.len = size;
    rd.ret = 0;

    hvt_do_hypercall(HVT_HYPERCALL_BLKREAD, &rd);

    return (rd.ret == 0 && rd.len == size) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_block_discard(solo5_off_t offset, size_t size)
{
    if (!block_info.capacity)
        init_block_info();

    if ((offset % block_info.block_size != 0) ||
        (size % block_info.block_size != 0) ||
        (offset >= block_info.capacity) ||
        (offset + size < offset) || /* checks for overflow */
        (offset + size > block_info.capacity))
        return SOLO5_R_EINVAL;

    volatile struct hvt_blkdiscard di;
    di.sector = offset / block_info.block_size;
    di.len_sectors = size / block_info.block_size;
    di.ret = 0;

    hvt_do_hypercall(HVT_HYPERCALL_BLKDISCARD, &di);

    return (di.ret == 0) ? SOLO5_R_OK : (di.ret == -2) ? SOLO5_R_EOPNOTSUPP : SOLO5_R_EUNSPEC;
}

void solo5_block_info(struct solo5_block_info *info)
{
    if (!block_info.capacity)
        init_block_info();

    *info = block_info;
}
