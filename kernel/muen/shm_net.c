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
#include "../ukvm/kernel.h"
#include "sinfo.h"
#include "reader.h"
#include "writer.h"
#include "shm_net.h"

shm_net_result_t shm_net_write(struct muchannel *channel,
        const uint8_t *buf, size_t size)
{
    struct net_msg pkt;

    if (size > PACKET_SIZE) {
        return SHM_NET_EINVAL;
    }

    memset(&pkt, 0, sizeof(struct net_msg));
    cc_barrier();
    pkt.length = size;
    memcpy(&pkt.data, buf, size);
    if (muen_channel_write(channel, &pkt) != 0) {
        return SHM_NET_AGAIN;
    }

    return SHM_NET_OK;
}

shm_net_result_t shm_net_read(struct muchannel *channel,
        struct muchannel_reader *reader,
        uint8_t *buf, size_t size, size_t *read_size)
{
    enum muchannel_reader_result result;
    struct net_msg pkt;

    if (size < PACKET_SIZE)
        return SHM_NET_EINVAL;

    result = muen_channel_read(channel, reader, &pkt);
    if (result == MUCHANNEL_SUCCESS || result == MUCHANNEL_XON) {
        memcpy(buf, &pkt.data, pkt.length);
        *read_size = pkt.length;
        if (result == MUCHANNEL_XON) {
            return SHM_NET_XON;
        }
        return SHM_NET_OK;
    } else if (result == MUCHANNEL_NO_DATA) {
        return SHM_NET_AGAIN;
    } else if (result == MUCHANNEL_EPOCH_CHANGED) {
        return SHM_NET_EPOCH_CHANGED;
    }
    return SHM_NET_EINVAL;
}
