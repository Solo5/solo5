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
#include "shm_net.h"
#include "sinfo.h"
#include "reader.h"
#include "writer.h"
#include "bindings.h"

struct muchannel *tx_channel;
struct muchannel *rx_channel;
struct muchannel_reader net_rdr;
bool shm_enabled = false;

static inline solo5_result_t shm_to_solo5_result(int shm_result)
{
    switch(shm_result) {
    case SHM_NET_OK:
    case SHM_NET_XON:
        return SOLO5_R_OK;
    case SHM_NET_AGAIN:
    case SHM_NET_EPOCH_CHANGED:
        return SOLO5_R_AGAIN;
    default:
        break;
    }
    return SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_net_queue(const uint8_t *buf, size_t size)
{
    int ret;

    assert(shm_enabled);
    ret = shm_net_write(tx_channel, buf, size);
    return shm_to_solo5_result(ret);
}

void solo5_net_flush()
{
    assert(shm_enabled);
    hvt_do_hypercall(HVT_HYPERCALL_NET_NOTIFY, NULL);
}

solo5_result_t solo5_net_write(const uint8_t *buf, size_t size)
{
    int ret = 0;
    if (shm_enabled) {
        ret = solo5_net_queue(buf, size);
        solo5_net_flush();
        return ret;
    } else {
        volatile struct hvt_netwrite wr;

        wr.data = buf;
        wr.len = size;
        wr.ret = 0;

        hvt_do_hypercall(HVT_HYPERCALL_NETWRITE, &wr);

        return (wr.ret == 0 && wr.len == size) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
    }
}

solo5_result_t solo5_net_read(uint8_t *buf, size_t size, size_t *read_size)
{
    int ret = 0;
    if (shm_enabled) {
        ret = shm_net_read(rx_channel, &net_rdr,
                buf, size, read_size);

        if (ret == SHM_NET_XON) {
            hvt_do_hypercall(HVT_HYPERCALL_NET_XON, NULL);
        }
        return shm_to_solo5_result(ret);
    } else {
        volatile struct hvt_netread rd;

        rd.data = buf;
        rd.len = size;
        rd.ret = 0;

        hvt_do_hypercall(HVT_HYPERCALL_NETREAD, &rd);

        *read_size = rd.len;
        return (rd.ret == 0) ? SOLO5_R_OK : SOLO5_R_AGAIN;
    }
}

void solo5_net_info(struct solo5_net_info *info)
{
    volatile struct hvt_netinfo ni;

    hvt_do_hypercall(HVT_HYPERCALL_NETINFO, &ni);

    memcpy(info->mac_address, (uint8_t *)&ni.mac_address,
            sizeof info->mac_address);
    /* XXX: No support on host side yet, so hardcode for now */
    info->mtu = 1500;
}

void net_init(void)
{
    volatile struct hvt_shm_info ni;
    hvt_do_hypercall(HVT_HYPERCALL_SHMINFO, &ni);

    shm_enabled = ni.shm_enabled;

    if (shm_enabled) {
        tx_channel = (struct muchannel *)ni.tx_channel_addr;
        rx_channel = (struct muchannel *)ni.rx_channel_addr;

        muen_channel_init_writer(tx_channel, MUENNET_PROTO, sizeof(struct net_msg),
                ni.tx_channel_addr_size, 10, 1);
        muen_channel_init_reader(&net_rdr, MUENNET_PROTO);
    }
}
