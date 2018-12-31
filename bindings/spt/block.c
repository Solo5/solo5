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

#pragma GCC diagnostic ignored "-Wunused-parameter"

static int block_fd = -1;
static solo5_off_t block_capacity;
static solo5_off_t block_size;

void block_init(struct spt_boot_info *bi)
{
    if (bi->blocki.present) {
        block_fd = bi->blocki.hostfd;
        block_capacity = bi->blocki.capacity;
        block_size = bi->blocki.block_size;
    }
}

void solo5_block_info(struct solo5_block_info *info)
{
    assert(block_fd >= 0);

    info->capacity = block_capacity;
    info->block_size = block_size;
}

solo5_result_t solo5_block_read(solo5_off_t offset, uint8_t *buf, size_t size)
{
    assert(block_fd >= 0);

    /* XXX: These need hard checks on the tender/seccomp side? */
    if(size != block_size)
        return SOLO5_R_EINVAL;
    if(offset & (block_size - 1))
        return SOLO5_R_EINVAL;
    if(offset > (block_capacity - block_size))
        return SOLO5_R_EINVAL;

    int nbytes = sys_pread64(block_fd, (char *)buf, size, offset);

    return (nbytes == (int)size) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_block_write(solo5_off_t offset, const uint8_t *buf,
        size_t size)
{
    assert(block_fd >= 0);
    
    /* XXX: These need hard checks on the tender/seccomp side? */
    if(size != block_size)
        return SOLO5_R_EINVAL;
    if(offset & (block_size - 1))
        return SOLO5_R_EINVAL;
    if(offset > (block_capacity - block_size))
        return SOLO5_R_EINVAL;
   
    int nbytes = sys_pwrite64(block_fd, (const char *)buf, size, offset);

    return (nbytes == (int)size) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}
