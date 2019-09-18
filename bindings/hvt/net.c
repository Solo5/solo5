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

solo5_result_t solo5_net_write(solo5_handle_t handle, const uint8_t *buf,
        size_t size)
{
    volatile struct hvt_hc_net_write wr;

    wr.handle = handle;
    wr.data = buf;
    wr.len = size;
    wr.ret = 0;

    hvt_do_hypercall(HVT_HYPERCALL_NET_WRITE, &wr);

    return wr.ret;
}

solo5_result_t solo5_net_read(solo5_handle_t handle, uint8_t *buf, size_t size,
        size_t *read_size)
{
    volatile struct hvt_hc_net_read rd;

    rd.handle = handle;
    rd.data = buf;
    rd.len = size;
    rd.ret = 0;

    hvt_do_hypercall(HVT_HYPERCALL_NET_READ, &rd);

    *read_size = rd.len;
    return rd.ret;
}

solo5_result_t solo5_net_acquire(const char *name, solo5_handle_t *handle,
        struct solo5_net_info *info)
{
    unsigned index;
    const struct mft_entry *e =
        mft_get_by_name(mft, name, MFT_DEV_NET_BASIC, &index);
    if (e == NULL)
        return SOLO5_R_EINVAL;
    assert(e->attached);

    *handle = index;
    info->mtu = e->u.net_basic.mtu;
    memcpy(info->mac_address, e->u.net_basic.mac,
            sizeof info->mac_address);
    return SOLO5_R_OK;
}

void net_init(const struct hvt_boot_info *bi)
{
    mft = bi->mft;
}
