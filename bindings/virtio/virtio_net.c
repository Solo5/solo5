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
#include "virtio_ring.h"
#include "virtio_pci.h"

/* The feature bitmap for virtio net */
#define VIRTIO_NET_F_CSUM       0 /* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM        1 /* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_MTU (1 << 3)
#define VIRTIO_NET_F_MAC (1 << 5) /* Host has given MAC address. */

#define PKT_BUFFER_LEN 1526
#define VIRTIO_NET_MTU 1500

extern struct mft *virtio_manifest;

struct virtio_net_desc {
    uint16_t pci_base; /* base in PCI config space */
    struct virtq xmitq;
    struct virtq recvq;
    uint8_t net_mac[6];
    uint16_t mtu;
    solo5_handle_t handle;
};

#define VIRTIO_NET_MAX_ENTRIES  MFT_MAX_ENTRIES
static struct virtio_net_desc nd_table[VIRTIO_NET_MAX_ENTRIES];
static unsigned nd_num_entries = 0;

#if MFT_MAX_ENTRIES > 64
#error "Can not hold more than 64 devices in virtio_set_t"
#endif

typedef uint64_t virtio_set_t;

#define VIRTQ_RECV 0
#define VIRTQ_XMIT 1

/* This header comes first in the scatter-gather list.
 * If VIRTIO_F_ANY_LAYOUT is not negotiated, it must
 * be the first element of the scatter-gather list.  If you don't
 * specify GSO or CSUM features, you can simply ignore the header.
 */
struct __attribute__((__packed__)) virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM     1   /* Use csum_start, csum_offset */
#define VIRTIO_NET_HDR_F_DATA_VALID     2        /* Csum is valid */
    uint8_t flags;
#define VIRTIO_NET_HDR_GSO_NONE         0        /* Not a GSO frame */
#define VIRTIO_NET_HDR_GSO_TCPV4        1        /* GSO frame, IPv4 TCP (TSO) */
#define VIRTIO_NET_HDR_GSO_UDP          3        /* GSO frame, IPv4 UDP (UFO) */
#define VIRTIO_NET_HDR_GSO_TCPV6        4        /* GSO frame, IPv6 TCP */
#define VIRTIO_NET_HDR_GSO_ECN       0x80        /* TCP has ECN set */
    uint8_t gso_type;
    uint16_t hdr_len;                /* Ethernet + IP + tcp/udp hdrs */
    uint16_t gso_size;                /* Bytes to append to hdr_len per frame */
    uint16_t csum_start;        /* Position to start checksumming from */
    uint16_t csum_offset;        /* Offset after that to place checksum */
};

static char virtio_net_mac_str[18];

static bool net_configured;

static int handle_virtio_net_interrupt(void *);

/* WARNING: called in interrupt context */
int handle_virtio_net_interrupt(void *arg)
{
    struct virtio_net_desc *nd = (struct virtio_net_desc *)arg;
    uint8_t isr_status;

    assert(nd != NULL);

    if (net_configured) {
        isr_status = inb(nd->pci_base + VIRTIO_PCI_ISR);
        if (isr_status & VIRTIO_PCI_ISR_HAS_INTR) {
            /* This interrupt is just to kick the application out of any
             * solo5_poll() that may be running. */
            return 1;
        }
    }
    return 0;
}

static void recv_setup(struct virtio_net_desc *nd)
{
    uint16_t mask = nd->recvq.num - 1;
    do {
        struct io_buffer *buf; /* header and data in a single descriptor */
        buf = &nd->recvq.bufs[nd->recvq.next_avail & mask];
        memset(buf->data, 0, PKT_BUFFER_LEN);
        buf->len = PKT_BUFFER_LEN;
        buf->extra_flags = VIRTQ_DESC_F_WRITE;
        assert(virtq_add_descriptor_chain(&nd->recvq,
                                          nd->recvq.next_avail & mask, 1) == 0);
    } while ((nd->recvq.next_avail & mask) != 0);

    outw(nd->pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_RECV);
}

/* performance note: we perform a copy into the xmit buffer */
int virtio_net_xmit_packet(struct virtio_net_desc *nd,
                           const void *data, size_t len)
{
    uint16_t mask = nd->xmitq.num - 1;
    uint16_t head;
    struct io_buffer *head_buf, *data_buf;
    int r;

    /* Consume used descriptors from all the previous tx'es. */
    for (; nd->xmitq.last_used != nd->xmitq.used->idx; nd->xmitq.last_used++)
        nd->xmitq.num_avail += 2; /* 2 descriptors per chain */

    /* next_avail is incremented by virtq_add_descriptor_chain below. */
    head = nd->xmitq.next_avail & mask;
    head_buf = &nd->xmitq.bufs[head];
    data_buf = &nd->xmitq.bufs[(head + 1) & mask];

    /* The header buf */
    memset(head_buf->data, 0, sizeof(struct virtio_net_hdr));
    head_buf->len = sizeof(struct virtio_net_hdr);
    head_buf->extra_flags = 0;

    /* The data buf */
    assert(len <= PKT_BUFFER_LEN);
    memcpy(data_buf->data, data, len);
    data_buf->len = len;
    data_buf->extra_flags = 0;

    r = virtq_add_descriptor_chain(&nd->xmitq, head, 2);

    outw(nd->pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_XMIT);

    return r;
}

static void virtio_net_config(struct virtio_net_desc *nd,
                              struct pci_config_info *pci)
{
    uint32_t host_features, guest_features;
    size_t pgs;

    /*
     * 3.1.1 Driver Requirements: Device Initialization
     *
     * 1. Reset the device.
     */
    outb(pci->base + VIRTIO_PCI_STATUS, 0);

    /*
     * 2. Set the ACKNOWLEDGE status bit: the guest OS has notice the device.
     * 3. Set the DRIVER status bit: the guest OS knows how to drive the device.
     */

    outb(pci->base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_ACK);
    outb(pci->base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_DRIVER);

    /*
     * 4. Read device feature bits, and write the subset of feature bits
     * understood by the OS and driver to the device. During this step the
     * driver MAY read (but MUST NOT write) the device-specific configuration
     * fields to check that it can support the device before accepting it.
     */

    host_features = inl(pci->base + VIRTIO_PCI_HOST_FEATURES);
    assert(host_features & VIRTIO_NET_F_MAC);

    /* only negotiate that the mac was set for now */
    guest_features = VIRTIO_NET_F_MAC;
    outl(pci->base + VIRTIO_PCI_GUEST_FEATURES, guest_features);

    for (int i = 0; i < 6; i++) {
        nd->net_mac[i] = inb(pci->base + VIRTIO_PCI_CONFIG_OFF + i);
    }

    /* the struct virtio_net_config contains:
       - 48 bit mac[6]
       - 16 bit status[2]
       - 16 bit max_virtqueue_pairs[2]
       - 16 bit mtu[2]
       So we read 16 bit at offset 10.
    */
    if (host_features & VIRTIO_NET_F_MTU) {
      nd->mtu = inw(pci->base + VIRTIO_PCI_CONFIG_OFF + 6 + 2 + 2);
    } else
      nd->mtu = VIRTIO_NET_MTU;

    snprintf(virtio_net_mac_str,
             sizeof(virtio_net_mac_str),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             nd->net_mac[0],
             nd->net_mac[1],
             nd->net_mac[2],
             nd->net_mac[3],
             nd->net_mac[4],
             nd->net_mac[5]);
    log(INFO, "Solo5: PCI:%02x:%02x: configured, mac=%s, mtu=%d, features=0x%x\n",
        pci->bus, pci->dev, virtio_net_mac_str, nd->mtu, host_features);

    /*
     * 7. Perform device-specific setup, including discovery of virtqueues for
     * the device, optional per-bus setup, reading and possibly writing the
     * device's virtio configuration space, and population of virtqueues.
     */

    virtq_init_rings(pci->base, &nd->recvq, VIRTQ_RECV);
    virtq_init_rings(pci->base, &nd->xmitq, VIRTQ_XMIT);

    pgs = (((nd->recvq.num * sizeof (struct io_buffer)) - 1) >> PAGE_SHIFT) + 1;
    nd->recvq.bufs = mem_ialloc_pages(pgs);
    assert(nd->recvq.bufs);
    memset(nd->recvq.bufs, 0, pgs << PAGE_SHIFT);

    pgs = (((nd->recvq.num * sizeof (struct io_buffer)) - 1) >> PAGE_SHIFT) + 1;
    nd->xmitq.bufs = mem_ialloc_pages(pgs);
    assert(nd->xmitq.bufs);
    memset(nd->xmitq.bufs, 0, pgs << PAGE_SHIFT);

    nd->pci_base = pci->base;
    net_configured = 1;
    intr_register_irq(pci->irq, handle_virtio_net_interrupt, nd);
    recv_setup(nd);

    /*
     * We don't need to get interrupts every time the device uses our
     * descriptors. Instead, we check for used packets in the transmit path of
     * following packets (as suggested in "5.1.6.2.1 Packet Transmission
     * Interrupt").
     */

    nd->xmitq.avail->flags |= VIRTQ_AVAIL_F_NO_INTERRUPT;

    /*
     * 8. Set the DRIVER_OK status bit. At this point the device is "live".
     */

    outb(pci->base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_DRIVER_OK);
}

/* Set ready for each net interface if there is pending data to read. */
static void virtio_net_pkt_poll(virtio_set_t *ready_set)
{
    unsigned idx;
    *ready_set = 0;

    if (!net_configured)
        return;

    assert(nd_num_entries < VIRTIO_NET_MAX_ENTRIES);

    for (idx = 0; idx < nd_num_entries; idx++) {
        struct virtio_net_desc *nd = &nd_table[idx];
        /* The device increments used->idx whenever it uses a packet (i.e. it
         * put a packet on our receive queue) and if it's ahead of last_used
         * it means that we have a pending packet. */
        if (nd->recvq.last_used != nd->recvq.used->idx)
            *ready_set |= 1ULL << idx;
    }
}

/* Get the data from the next_avail (top-most) receive buffer/descriptpr in
 * the available ring. */
static uint8_t *virtio_net_recv_pkt_get(struct virtio_net_desc *nd,
        size_t *size)
{
    uint16_t mask = nd->recvq.num - 1;
    struct virtq_used_elem *e;
    struct io_buffer *buf;
    uint16_t desc_idx;

    /* The device increments used->idx whenever it uses a packet (i.e. it put
     * a packet on our receive queue) and if it's ahead of last_used it means
     * that we have a pending packet. */
    if (nd->recvq.last_used == nd->recvq.used->idx)
        return NULL;

    e = &(nd->recvq.used->ring[nd->recvq.last_used & mask]);
    desc_idx = e->id;

    buf = (struct io_buffer *) nd->recvq.desc[desc_idx].addr;
    buf->len = e->len;

    /* Remove the virtio_net_hdr */
    *size = buf->len - sizeof(struct virtio_net_hdr);
    return buf->data + sizeof(struct virtio_net_hdr);
}

/* Return the next_avail (top-most) receive buffer/descriptor to the available
 * ring. */
static void virtio_net_recv_pkt_put(struct virtio_net_desc *nd)
{
    uint16_t mask = nd->recvq.num - 1;
    nd->recvq.bufs[nd->recvq.next_avail & mask].len = PKT_BUFFER_LEN;
    nd->recvq.bufs[nd->recvq.next_avail & mask].extra_flags = VIRTQ_DESC_F_WRITE;

    /* This sets the returned descriptor to be ready for incoming packets, and
     * advances the next_avail index. */
    assert(virtq_add_descriptor_chain(&nd->recvq, nd->recvq.next_avail & mask, 1) == 0);
    outw(nd->pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_RECV);
}

int virtio_config_network(struct pci_config_info *pci, solo5_handle_t mft_index)
{
    unsigned nd_index = nd_num_entries;

    if (nd_index >= MFT_MAX_ENTRIES) {
        log(WARN, "Solo5: Virtio net: PCI:%02x:%02x not configured: "
            "too many devices\n", pci->bus, pci->dev);
        return -1;
    }

    struct mft_entry *e = mft_get_by_index(virtio_manifest,
            mft_index, MFT_DEV_NET_BASIC);
    if (e == NULL) {
        log(WARN, "Solo5: Virtio net: PCI:%02x:%02x not in manifest\n",
             pci->bus, pci->dev);
        return -1;
    }

    struct virtio_net_desc *nd = &nd_table[nd_index];

    virtio_net_config(nd, pci);
    e->u.net_basic.mtu = nd->mtu;
    e->b.hostfd = nd_index;
    memcpy(e->u.net_basic.mac, nd->net_mac, sizeof e->u.net_basic.mac);
    /*
     * We need to map virtio net descriptors to solo5 handles in order to
     * convert virtio ready sets to solo5 ready sets (in virtio_to_handle_set).
     */
    nd->handle = mft_index;
    e->attached = true;

    nd_num_entries++;
    return 0;
}

solo5_result_t solo5_net_acquire(const char *name, solo5_handle_t *h,
        struct solo5_net_info *info)
{
    unsigned mft_index;
    struct mft_entry *mft_e;

    if (!net_configured)
        return SOLO5_R_EUNSPEC;

    mft_e = mft_get_by_name(virtio_manifest, name, MFT_DEV_NET_BASIC, &mft_index);
    if (mft_e == NULL)
        return SOLO5_R_EINVAL;

    memcpy(info->mac_address, mft_e->u.net_basic.mac, sizeof info->mac_address);
    info->mtu = mft_e->u.net_basic.mtu;
    *h = mft_index;
    log(INFO, "Solo5: Application acquired '%s' as network device\n", name);
    return SOLO5_R_OK;
}

/*
 * Convert the set of virtio devices to a set of solo5 handles.
 * XXX: consider filtering out the handles that were not acquired.
 */
solo5_handle_set_t virtio_to_handle_set(virtio_set_t virtio_set)
{
    solo5_handle_set_t handle_set = 0;
    unsigned idx;

    assert(nd_num_entries < VIRTIO_NET_MAX_ENTRIES);

    for (idx = 0; idx < nd_num_entries; idx++) {
        if (virtio_set & (1ULL << idx)) {
            solo5_handle_t handle = nd_table[idx].handle;
            handle_set |= 1ULL << handle;
        }
    }
    return handle_set;
}

void solo5_yield(solo5_time_t deadline, solo5_handle_set_t *ready_set)
{
    virtio_set_t virtio_set = 0;

    /*
     * cpu_block() as currently implemented will only poll for the maximum time
     * the PIT can be run in "one shot" mode. Loop until either I/O is possible
     * or the desired time has been reached.
     */
    cpu_intr_disable();
    do {
        virtio_net_pkt_poll(&virtio_set);
        if (virtio_set != 0)
            break;

        cpu_block(deadline);
    } while (solo5_clock_monotonic() < deadline);
    if (virtio_set == 0)
        virtio_net_pkt_poll(&virtio_set);
    cpu_intr_enable();

    solo5_handle_set_t tmp_ready_set;
    if (virtio_set != 0)
        tmp_ready_set = virtio_to_handle_set(virtio_set);
    else
        tmp_ready_set = 0;

    if (ready_set)
        *ready_set = tmp_ready_set;
}

solo5_result_t solo5_net_write(solo5_handle_t h, const uint8_t *buf,
        size_t size)
{
    struct mft_entry *e = mft_get_by_index(virtio_manifest, h, MFT_DEV_NET_BASIC);
    if (e == NULL)
        return SOLO5_R_EINVAL;

    assert(e->attached);
    assert(e->b.hostfd < VIRTIO_NET_MAX_ENTRIES);

    int rv = virtio_net_xmit_packet(&nd_table[e->b.hostfd], buf, size);
    return (rv == 0) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}

/* Returns 0 if a packet was read, -1 if there is there is no pending packet. */
static int virtio_net_recv(struct virtio_net_desc *nd, uint8_t *buf,
        size_t size, size_t *read_size)
{
    uint8_t *pkt;
    size_t len = size;


    /* We only need interrupts to wake up the application when it's sleeping
     * and waiting for incoming packets. The app is definitely not doing that
     * now (as we are here), so disable them. */

    nd->recvq.avail->flags |= VIRTQ_AVAIL_F_NO_INTERRUPT;

    pkt = virtio_net_recv_pkt_get(nd, &len);
    if (!pkt) {
        nd->recvq.avail->flags &= ~VIRTQ_AVAIL_F_NO_INTERRUPT;
        return -1;
    }

    assert(len <= size);
    assert(len <= PKT_BUFFER_LEN);
    *read_size = len;
    /* also, it's clearly not zero copy */
    memcpy(buf, pkt, len);

    /* Consume the recently used descriptor. */
    nd->recvq.last_used++;
    nd->recvq.num_avail++;

    virtio_net_recv_pkt_put(nd);

    nd->recvq.avail->flags &= ~VIRTQ_AVAIL_F_NO_INTERRUPT;

    return 0;
}

solo5_result_t solo5_net_read(solo5_handle_t h, uint8_t *buf, size_t size,
        size_t *read_size)
{
    struct mft_entry *e = mft_get_by_index(virtio_manifest, h, MFT_DEV_NET_BASIC);
    if (e == NULL)
        return SOLO5_R_EINVAL;

    assert(e->attached);
    assert(e->b.hostfd < VIRTIO_NET_MAX_ENTRIES);

    int rv = virtio_net_recv(&nd_table[e->b.hostfd], buf, size, read_size);
    return (rv == 0) ? SOLO5_R_OK : SOLO5_R_AGAIN;
}
