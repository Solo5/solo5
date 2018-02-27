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

solo5_result_t solo5_net_write(const uint8_t *buf, size_t size)
{
    volatile struct ukvm_netwrite wr;

    wr.data = buf;
    wr.len = size;
    wr.ret = 0;

    ukvm_do_hypercall(UKVM_HYPERCALL_NETWRITE, &wr);

    return (wr.ret == 0 && wr.len == size) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_net_read(uint8_t *buf, size_t size, size_t *read_size)
{
    volatile struct ukvm_netread rd;

    rd.data = buf;
    rd.len = size;
    rd.ret = 0;

    ukvm_do_hypercall(UKVM_HYPERCALL_NETREAD, &rd);

    *read_size = rd.len;
    return (rd.ret == 0) ? SOLO5_R_OK : SOLO5_R_AGAIN;
}

void solo5_net_info(struct solo5_net_info *info)
{
    volatile struct ukvm_netinfo ni;

    ukvm_do_hypercall(UKVM_HYPERCALL_NETINFO, &ni);

    memcpy(info->mac_address, (uint8_t *)&ni.mac_address,
            sizeof info->mac_address);
    /* XXX: No support on host side yet, so hardcode for now */
    info->mtu = 1500;
}

void net_init(void)
{
}
