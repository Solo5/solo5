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
#include "virtio_ring.h"
#include "virtio_pci.h"

/* The feature bitmap for virtio net */
#define VIRTIO_NET_F_CSUM       0 /* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM	1 /* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_MAC (1 << 5) /* Host has given MAC address. */

#define PKT_BUFFER_LEN 1526

static struct virtq recvq;
static struct virtq xmitq;

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

static uint16_t virtio_net_pci_base; /* base in PCI config space */

static uint8_t virtio_net_mac[6];
static char virtio_net_mac_str[18];

static int net_configured;

static int handle_virtio_net_interrupt(void *);

/* WARNING: called in interrupt context */
int handle_virtio_net_interrupt(void *arg __attribute__((unused)))
{
    uint8_t isr_status;

    if (net_configured) {
        isr_status = inb(virtio_net_pci_base + VIRTIO_PCI_ISR);
        if (isr_status & VIRTIO_PCI_ISR_HAS_INTR) {
	    /* This interrupt is just to kick the application out of any
             * solo5_poll() that may be running. */
            return 1;
        }
    }
    return 0;
}

static void recv_setup(void)
{
    uint16_t mask = recvq.num - 1;
    do {
        struct io_buffer *buf; /* header and data in a single descriptor */
        buf = &recvq.bufs[recvq.next_avail & mask];
        memset(buf->data, 0, PKT_BUFFER_LEN);
        buf->len = PKT_BUFFER_LEN;
        buf->extra_flags = VIRTQ_DESC_F_WRITE;
        assert(virtq_add_descriptor_chain(&recvq,
                                          recvq.next_avail & mask, 1) == 0);
    } while ((recvq.next_avail & mask) != 0);

    outw(virtio_net_pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_RECV);
}

/* performance note: we perform a copy into the xmit buffer */
int virtio_net_xmit_packet(const void *data, size_t len)
{
    uint16_t mask = xmitq.num - 1;
    uint16_t head;
    struct io_buffer *head_buf, *data_buf;
    int r;

    /* Consume used descriptors from all the previous tx'es. */
    for (; xmitq.last_used != xmitq.used->idx; xmitq.last_used++)
        xmitq.num_avail += 2; /* 2 descriptors per chain */

    /* next_avail is incremented by virtq_add_descriptor_chain below. */
    head = xmitq.next_avail & mask;
    head_buf = &xmitq.bufs[head];
    data_buf = &xmitq.bufs[(head + 1) & mask];

    /* The header buf */
    memset(head_buf->data, 0, sizeof(struct virtio_net_hdr));
    head_buf->len = sizeof(struct virtio_net_hdr);
    head_buf->extra_flags = 0;

    /* The data buf */
    assert(len <= PKT_BUFFER_LEN);
    memcpy(data_buf->data, data, len);
    data_buf->len = len;
    data_buf->extra_flags = 0;

    r = virtq_add_descriptor_chain(&xmitq, head, 2);

    outw(virtio_net_pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_XMIT);

    return r;
}

void virtio_config_network(struct pci_config_info *pci)
{
    uint32_t host_features, guest_features;
    size_t pgs;

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
        virtio_net_mac[i] = inb(pci->base + VIRTIO_PCI_CONFIG_OFF + i);
    }
    snprintf(virtio_net_mac_str,
             sizeof(virtio_net_mac_str),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             virtio_net_mac[0],
             virtio_net_mac[1],
             virtio_net_mac[2],
             virtio_net_mac[3],
             virtio_net_mac[4],
             virtio_net_mac[5]);
    log(INFO, "Solo5: PCI:%02x:%02x: configured, mac=%s, features=0x%x\n",
        pci->bus, pci->dev, virtio_net_mac_str, host_features);

    /*
     * 7. Perform device-specific setup, including discovery of virtqueues for
     * the device, optional per-bus setup, reading and possibly writing the
     * device's virtio configuration space, and population of virtqueues.
     */

    virtq_init_rings(pci->base, &recvq, VIRTQ_RECV);
    virtq_init_rings(pci->base, &xmitq, VIRTQ_XMIT);

    pgs = (((recvq.num * sizeof (struct io_buffer)) - 1) >> PAGE_SHIFT) + 1;
    recvq.bufs = mem_ialloc_pages(pgs);
    assert(recvq.bufs);
    memset(recvq.bufs, 0, pgs << PAGE_SHIFT);

    pgs = (((recvq.num * sizeof (struct io_buffer)) - 1) >> PAGE_SHIFT) + 1;
    xmitq.bufs = mem_ialloc_pages(pgs);
    assert(xmitq.bufs);
    memset(xmitq.bufs, 0, pgs << PAGE_SHIFT);

    virtio_net_pci_base = pci->base;
    net_configured = 1;
    intr_register_irq(pci->irq, handle_virtio_net_interrupt, NULL);
    recv_setup();

    /*
     * We don't need to get interrupts every time the device uses our
     * descriptors. Instead, we check for used packets in the transmit path of
     * following packets (as suggested in "5.1.6.2.1 Packet Transmission
     * Interrupt").
     */

    xmitq.avail->flags |= VIRTQ_AVAIL_F_NO_INTERRUPT;

    /*
     * 8. Set the DRIVER_OK status bit. At this point the device is "live".
     */

    outb(pci->base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_DRIVER_OK);
}

/* Returns 1 if there is a pending used descriptor for us to read. */
int virtio_net_pkt_poll(void)
{
    if (!net_configured)
        return 0;

    /* The device increments used->idx whenever it uses a packet (i.e. it put
     * a packet on our receive queue) and if it's ahead of last_used it means
     * that we have a pending packet. */
    if (recvq.last_used == recvq.used->idx)
        return 0;
    else
        return 1;
}

/* Get the data from the next_avail (top-most) receive buffer/descriptpr in
 * the available ring. */
uint8_t *virtio_net_recv_pkt_get(size_t *size)
{
    uint16_t mask = recvq.num - 1;
    struct virtq_used_elem *e;
    struct io_buffer *buf;
    uint16_t desc_idx;

    /* The device increments used->idx whenever it uses a packet (i.e. it put
     * a packet on our receive queue) and if it's ahead of last_used it means
     * that we have a pending packet. */
    if (recvq.last_used == recvq.used->idx)
        return NULL;

    e = &(recvq.used->ring[recvq.last_used & mask]);
    desc_idx = e->id;

    buf = (struct io_buffer *) recvq.desc[desc_idx].addr;
    buf->len = e->len;

    /* Remove the virtio_net_hdr */
    *size = buf->len - sizeof(struct virtio_net_hdr);
    return buf->data + sizeof(struct virtio_net_hdr);
}

/* Return the next_avail (top-most) receive buffer/descriptor to the available
 * ring. */
void virtio_net_recv_pkt_put(void)
{
    uint16_t mask = recvq.num - 1;
    recvq.bufs[recvq.next_avail & mask].len = PKT_BUFFER_LEN;
    recvq.bufs[recvq.next_avail & mask].extra_flags = VIRTQ_DESC_F_WRITE;

    /* This sets the returned descriptor to be ready for incoming packets, and
     * advances the next_avail index. */
    assert(virtq_add_descriptor_chain(&recvq, recvq.next_avail & mask, 1) == 0);
    outw(virtio_net_pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_RECV);
}

solo5_result_t solo5_net_write(const uint8_t *buf, size_t size)
{
    assert(net_configured);

    int rv = virtio_net_xmit_packet(buf, size);
    return (rv == 0) ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
}

solo5_result_t solo5_net_read(uint8_t *buf, size_t size, size_t *read_size)
{
    uint8_t *pkt;
    size_t len = size;

    assert(net_configured);

    /* We only need interrupts to wake up the application when it's sleeping
     * and waiting for incoming packets. The app is definitely not doing that
     * now (as we are here), so disable them. */
    recvq.avail->flags |= VIRTQ_AVAIL_F_NO_INTERRUPT;

    pkt = virtio_net_recv_pkt_get(&len);
    if (!pkt) {
        recvq.avail->flags &= ~VIRTQ_AVAIL_F_NO_INTERRUPT;
        return SOLO5_R_AGAIN;
    }

    assert(len <= size);
    assert(len <= PKT_BUFFER_LEN);
    *read_size = len;

    /* also, it's clearly not zero copy */
    memcpy(buf, pkt, len);

    /* Consume the recently used descriptor. */
    recvq.last_used++;
    recvq.num_avail++;

    virtio_net_recv_pkt_put();

    recvq.avail->flags &= ~VIRTQ_AVAIL_F_NO_INTERRUPT;

    return SOLO5_R_OK;
}

void solo5_net_info(struct solo5_net_info *info)
{
    assert(net_configured);

    memcpy(info->mac_address, virtio_net_mac, sizeof info->mac_address);
    info->mtu = 1500;
}
