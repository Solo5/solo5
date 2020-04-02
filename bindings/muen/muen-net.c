/*
 * Copyright (c) 2017 Contributors as noted in the AUTHORS file
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
#include "sinfo.h"
#include "reader.h"
#include "writer.h"

#define PACKET_SIZE   1514
#define MUENNET_PROTO 0x7ade5c549b08e814ULL

struct net_msg {
    uint8_t data[PACKET_SIZE];
    uint16_t length;
} __attribute__((packed));

struct muen_net_device {
    bool acquired;
    uint8_t mac_addr[SOLO5_NET_ALEN];
    struct muchannel *net_in;
    struct muchannel *net_out;
    struct muchannel_reader net_rdr;
};

extern const struct mft *muen_manifest;

static struct muen_net_device net_devices[MFT_MAX_ENTRIES];

/**
 * Initialize Muen network device with given name.
 */
static bool muen_net_dev_init(const char *name, struct muen_net_device *device,
        struct solo5_net_info *info);

solo5_result_t solo5_net_write(solo5_handle_t handle,
        const uint8_t *buf, size_t size)
{
    struct net_msg pkt;

    if (handle >= MFT_MAX_ENTRIES || !net_devices[handle].acquired)
        return SOLO5_R_EINVAL;

    if (size > PACKET_SIZE)
        return SOLO5_R_EINVAL;

    memset(&pkt, 0, sizeof(struct net_msg));
    cc_barrier();
    pkt.length = size;
    memcpy(&pkt.data, buf, size);
    muen_channel_write(net_devices[handle].net_out, &pkt);

    return SOLO5_R_OK;
}

solo5_result_t solo5_net_read(solo5_handle_t handle,
        uint8_t *buf, size_t size, size_t *read_size)
{
    enum muchannel_reader_result result;
    struct net_msg pkt;

    if (handle >= MFT_MAX_ENTRIES || !net_devices[handle].acquired)
        return SOLO5_R_EINVAL;

    if (size < PACKET_SIZE)
        return SOLO5_R_EINVAL;

    result = muen_channel_read(net_devices[handle].net_in,
                               &net_devices[handle].net_rdr, &pkt);
    if (result == MUCHANNEL_SUCCESS) {
        memcpy(buf, &pkt.data, pkt.length);
        *read_size = pkt.length;
        return SOLO5_R_OK;
    } else {
        return SOLO5_R_AGAIN;
    }
}

solo5_result_t solo5_net_acquire(const char *name, solo5_handle_t *h,
        struct solo5_net_info *info)
{
    solo5_handle_t handle = 0;
    unsigned mft_index;
    const struct mft_entry *mft_e = mft_get_by_name(muen_manifest, name,
            MFT_DEV_NET_BASIC, &mft_index);
    if (mft_e == NULL)
        return SOLO5_R_EINVAL;

    handle = mft_index;

    if (net_devices[handle].acquired) {
        log(WARN, "Solo5: Net: Device '%s' already acquired\n", name);
        return SOLO5_R_EINVAL;
    }

    if (!muen_net_dev_init(name, &net_devices[handle], info))
        return SOLO5_R_EINVAL;

    *h = handle;
    net_devices[handle].acquired = true;
    log(INFO, "Solo5: Application acquired '%s' as network device\n", name);
    return SOLO5_R_OK;
}

bool muen_net_pending_data(solo5_handle_t handle)
{
    return handle < MFT_MAX_ENTRIES
        && net_devices[handle].acquired
        && muen_channel_has_pending_data(net_devices[handle].net_in,
                                         &net_devices[handle].net_rdr);
}

static void generate_mac_addr(uint8_t *addr)
{
    const uint64_t dsize = sizeof(uint64_t) * 8;
    const char *subject_name = muen_get_subject_name();
    static uint64_t counter = 0;
    uint64_t data;
    int i;

    data  = (muen_get_sched_start() << 32) | muen_get_sched_end();
    data ^= tscclock_epochoffset();
    data  = ((data << counter) | (data >> (dsize - counter))) + counter;
    counter = (counter + 1) % dsize;

    for (i = 0; i < 6; i++)
    {
        addr[i]  = subject_name[i];
        addr[i] ^= (uint8_t)(data >> (i * 8));
    }

    /* clear multicast and set local assignment bit */
    addr[0] &= 0xfe;
    addr[0] |= 0x02;
}

static bool muen_net_dev_init(const char *name, struct muen_net_device *device,
        struct solo5_net_info *info)
{
    assert(!device->acquired);
    const uint64_t epoch = muen_get_sched_start();
    char mac_str[18];
    char buffer[64];
    const int retval = snprintf(buffer, sizeof buffer, "%s|out", name);
    assert(retval > 0 && (unsigned)retval < sizeof buffer);
    const struct muen_resource_type *const
        chan_out = muen_get_resource(buffer, MUEN_RES_MEMORY);
	snprintf(buffer, sizeof buffer, "%s|in", name);
    const struct muen_resource_type *const
        chan_in = muen_get_resource(buffer, MUEN_RES_MEMORY);

    if (!chan_out) {
        log(WARN, "Solo5: Net: '%s': No output channel\n", name);
        return false;
    }
    if (chan_out->data.mem.kind != MUEN_MEM_SUBJ_CHANNEL) {
        log(WARN, "Solo5: Net: '%s': Memory '%s' is not a channel\n",
            name, chan_out->name.data);
        return false;
    }
    if (!(chan_out->data.mem.flags & MEM_WRITABLE_FLAG)) {
        log(WARN, "Solo5: Net: '%s': Output channel '%s' not writable\n",
            name, chan_out->name.data);
        return false;
    }

    if (!chan_in) {
        log(WARN, "Solo5: Net: '%s': No input channel\n", name);
        return false;
    }
    if (chan_in->data.mem.kind != MUEN_MEM_SUBJ_CHANNEL) {
        log(WARN, "Solo5: Net: '%s': Memory '%s' is not a channel\n",
            name, chan_in->name.data);
        return false;
    }
    if (chan_in->data.mem.flags & MEM_WRITABLE_FLAG) {
        log(DEBUG, "Solo5: Net: '%s': Input channel '%s' is writable\n",
            name, chan_in->name.data);
    }

    device->net_out = (struct muchannel *)(chan_out->data.mem.address);
    muen_channel_init_writer(device->net_out, MUENNET_PROTO, sizeof(struct net_msg),
                             chan_out->data.mem.size, epoch);
    log(INFO, "Solo5: Net: '%s': Muen shared memory stream, protocol 0x%llx\n",
        name, MUENNET_PROTO);
    log(INFO, "Solo5: Net: '%s': Output channel @ 0x%llx, size 0x%llx, epoch 0x%llx\n",
        name,
        (unsigned long long)chan_out->data.mem.address,
        (unsigned long long)chan_out->data.mem.size, (unsigned long long)epoch);

    device->net_in = (struct muchannel *)(chan_in->data.mem.address);
    muen_channel_init_reader(&device->net_rdr, MUENNET_PROTO);
    log(INFO, "Solo5: Net: '%s': Input  channel @ 0x%llx, size 0x%llx\n",
        name,
        (unsigned long long)chan_in->data.mem.address,
        (unsigned long long)chan_in->data.mem.size);

    generate_mac_addr(device->mac_addr);
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             device->mac_addr[0], device->mac_addr[1], device->mac_addr[2],
             device->mac_addr[3], device->mac_addr[4], device->mac_addr[5]);
    memcpy(info->mac_address, device->mac_addr, sizeof info->mac_address);
    info->mtu = 1500;
    log(INFO, "Solo5: Net: '%s': Using MAC address %s\n", name, mac_str);
    return true;
}

void net_init(const struct hvt_boot_info *bi __attribute__((unused)))
{
    for (solo5_handle_t i = 0U; i < MFT_MAX_ENTRIES; ++i) {
        net_devices[i].acquired = false;
        net_devices[i].net_in = NULL;
        net_devices[i].net_out = NULL;
    }
}
