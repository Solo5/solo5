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

#include "bindings.h"

static struct mft *mft;

solo5_result_t solo5_block_write(solo5_handle_t handle, solo5_off_t offset,
        const uint8_t *buf, size_t size)
{
    volatile struct hvt_blkwrite wr;
    wr.handle = handle;
    wr.offset = offset;
    wr.data = buf;
    wr.len = size;
    wr.ret = 0;

    hvt_do_hypercall(HVT_HYPERCALL_BLKWRITE, &wr);

    return wr.ret;
}

solo5_result_t solo5_block_read(solo5_handle_t handle, solo5_off_t offset,
        uint8_t *buf, size_t size)
{
    volatile struct hvt_blkread rd;
    rd.handle = handle;
    rd.offset = offset;
    rd.data = buf;
    rd.len = size;
    rd.ret = 0;

    hvt_do_hypercall(HVT_HYPERCALL_BLKREAD, &rd);

    return rd.ret;
}

solo5_result_t solo5_block_acquire(const char *name, solo5_handle_t *handle,
        struct solo5_block_info *info)
{
    unsigned index;
    struct mft_entry *e = mft_get_by_name(mft, name, MFT_BLOCK_BASIC, &index);
    if (e == NULL)
        return SOLO5_R_EINVAL;
    assert(e->ok);

    *handle = index;
    info->capacity = e->u.block_basic.capacity;
    info->block_size = e->u.block_basic.block_size;
    return SOLO5_R_OK;
}

void block_init(struct hvt_boot_info *bi)
{
    mft = bi->mft;
}
