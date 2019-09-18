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

static const struct mft *mft;

void block_init(struct spt_boot_info *bi)
{
    mft = bi->mft;
}

solo5_result_t solo5_block_acquire(const char *name, solo5_handle_t *handle,
        struct solo5_block_info *info)
{
    unsigned index;
    const struct mft_entry *e =
        mft_get_by_name(mft, name, MFT_DEV_BLOCK_BASIC, &index);
    if (e == NULL)
        return SOLO5_R_EINVAL;
    assert(e->attached);

    *handle = index;
    info->capacity = e->u.block_basic.capacity;
    info->block_size = e->u.block_basic.block_size;
    return SOLO5_R_OK;
}

solo5_result_t solo5_block_read(solo5_handle_t handle, solo5_off_t offset,
        uint8_t *buf, size_t size)
{
    const struct mft_entry *e =
        mft_get_by_index(mft, handle, MFT_DEV_BLOCK_BASIC);
    if (e == NULL)
        return SOLO5_R_EINVAL;

    /*
     * Note that reads beyond capacity are additionally enforced by the
     * tender's seccomp policy.
     */
    if(size != e->u.block_basic.block_size)
        return SOLO5_R_EINVAL;
    if(offset & (e->u.block_basic.block_size - 1))
        return SOLO5_R_EINVAL;
    if(offset > (e->u.block_basic.capacity - e->u.block_basic.block_size))
        return SOLO5_R_EINVAL;

    long nbytes = sys_pread64(e->b.hostfd, (char *)buf, size, offset);

    return (nbytes == (int)size) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_block_write(solo5_handle_t handle, solo5_off_t offset,
        const uint8_t *buf, size_t size)
{
    const struct mft_entry *e =
        mft_get_by_index(mft, handle, MFT_DEV_BLOCK_BASIC);
    if (e == NULL)
        return SOLO5_R_EINVAL;

    /*
     * Note that writes beyond capacity are additionally enforced by the
     * tender's seccomp policy.
     */
    if(size != e->u.block_basic.block_size)
        return SOLO5_R_EINVAL;
    if(offset & (e->u.block_basic.block_size - 1))
        return SOLO5_R_EINVAL;
    if(offset > (e->u.block_basic.capacity - e->u.block_basic.block_size))
        return SOLO5_R_EINVAL;

    long nbytes = sys_pwrite64(e->b.hostfd, (const char *)buf, size, offset);

    return (nbytes == (int)size) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}
