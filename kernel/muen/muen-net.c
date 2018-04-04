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
#include "shm_net.h"
#include "sinfo.h"
#include "reader.h"
#include "writer.h"

struct muchannel *net_in;
struct muchannel *net_out;
struct muchannel_reader net_rdr;
static uint8_t mac_addr[6];

solo5_result_t solo5_net_write(const uint8_t *buf, size_t size)
{
    return shm_net_write(net_out, buf, size);
}

solo5_result_t solo5_net_read(uint8_t *buf, size_t size, size_t *read_size)
{
    return shm_net_read(net_in, &net_rdr, buf, size, read_size);
}

bool muen_net_pending_data()
{
    return muen_channel_has_pending_data(net_in, &net_rdr);
}

/* TODO: Support configured MAC address */
void solo5_net_info(struct solo5_net_info *info)
{
    memcpy(info->mac_address, mac_addr, sizeof info->mac_address);
    info->mtu = 1500;
}

void generate_mac_addr(uint8_t *addr)
{
    const char *subject_name = muen_get_subject_name();
    uint64_t data;
    int i;

    data  = (muen_get_sched_start() << 32) | muen_get_sched_end();
    data ^= tscclock_epochoffset();

    for (i = 0; i < 6; i++)
    {
        addr[i]  = subject_name[i];
        addr[i] ^= (uint8_t)(data >> (i * 8));
    }

    /* clear multicast and set local assignment bit */
    addr[0] &= 0xfe;
    addr[0] |= 0x02;
}

void net_init(void)
{
    char mac_str[18];
    struct muen_channel_info channel;
    const uint64_t epoch = muen_get_sched_start();

    if (!muen_get_channel_info("net_out", &channel)) {
        log(WARN, "Solo5: Net: No output channel\n");
        return;
    }
    net_out  = (struct muchannel *)(channel.address);
    muen_channel_init_writer(net_out, MUENNET_PROTO, sizeof(struct net_msg),
                             channel.size, epoch, 0);
    log(INFO, "Solo5: Net: Muen shared memory stream, protocol 0x%lx\n",
        MUENNET_PROTO);
    log(INFO, "Solo5: Net: Output channel @ 0x%lx, size 0x%lx, epoch 0x%lx\n",
        channel.address, channel.size, epoch);

    if (!muen_get_channel_info("net_in", &channel)) {
        log(WARN, "Solo5: Net: No input channel\n");
        return;
    }
    net_in = (struct muchannel *)(channel.address);
    muen_channel_init_reader(&net_rdr, MUENNET_PROTO);
    log(INFO, "Solo5: Net: Input  Channel @ 0x%lx, size 0x%lx\n", channel.address,
        channel.size);

    generate_mac_addr(mac_addr);
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);
    log(INFO, "Solo5: Net: Using MAC address %s\n", mac_str);
}
