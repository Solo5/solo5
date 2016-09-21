/* Copyright (c) 2015, IBM
 * Author(s): Dan Williams <djwillia@us.ibm.com>
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
            virtq_handle_interrupt(&xmitq);
            virtq_handle_interrupt(&recvq);
            return 1;
        }
    }
    return 0;
}

static void recv_setup(void)
{
    do {
        struct io_buffer *buf; /* header and data in a single descriptor */
        buf = &recvq.bufs[recvq.next_avail];
        memset(buf->data, 0, PKT_BUFFER_LEN);
        buf->len = PKT_BUFFER_LEN;
        buf->extra_flags = VIRTQ_DESC_F_WRITE;
        assert(virtq_add_descriptor_chain(&recvq, recvq.next_avail, 1) == 0);
    } while (recvq.next_avail != 0);

    outw(virtio_net_pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_RECV);
}

/* performance note: we perform a copy into the xmit buffer */
int virtio_net_xmit_packet(void *data, int len)
{
    uint16_t head = xmitq.next_avail;
    struct io_buffer *head_buf, *data_buf;
    int r;

    head_buf = &xmitq.bufs[head];
    data_buf = &xmitq.bufs[(head + 1) % xmitq.num];

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


void virtio_config_network(uint16_t base, unsigned irq)
{
    uint32_t host_features, guest_features;
    int i;
    int dbg = 0;

    /*
     * 2. Set the ACKNOWLEDGE status bit: the guest OS has notice the device.
     * 3. Set the DRIVER status bit: the guest OS knows how to drive the device. 
     */

    outb(base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_ACK);
    outb(base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_DRIVER);

    /*
     * 4. Read device feature bits, and write the subset of feature bits
     * understood by the OS and driver to the device. During this step the
     * driver MAY read (but MUST NOT write) the device-specific configuration
     * fields to check that it can support the device before accepting it. 
     */

    host_features = inl(base + VIRTIO_PCI_HOST_FEATURES);

    if (dbg) {
        uint32_t hf = host_features;

        printf("host features: %x: ", hf);
        for (i = 0; i < 32; i++) {
            if (hf & 0x1)
                printf("%d ", i);
            hf = hf >> 1;
        }
        printf("\n");
    }

    assert(host_features & VIRTIO_NET_F_MAC);

    /* only negotiate that the mac was set for now */
    guest_features = VIRTIO_NET_F_MAC;
    outl(base + VIRTIO_PCI_GUEST_FEATURES, guest_features);

    printf("Found virtio network device with MAC: ");
    for (i = 0; i < 6; i++) {
        virtio_net_mac[i] = inb(base + VIRTIO_PCI_CONFIG_OFF + i);
        printf("%02x ", virtio_net_mac[i]);
    }
    printf("\n");
    snprintf(virtio_net_mac_str,
             sizeof(virtio_net_mac_str),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             virtio_net_mac[0],
             virtio_net_mac[1],
             virtio_net_mac[2],
             virtio_net_mac[3],
             virtio_net_mac[4],
             virtio_net_mac[5]);

    /*
     * 7. Perform device-specific setup, including discovery of virtqueues for
     * the device, optional per-bus setup, reading and possibly writing the
     * device's virtio configuration space, and population of virtqueues.
     */

    virtq_init_rings(base, &recvq, VIRTQ_RECV);
    virtq_init_rings(base, &xmitq, VIRTQ_XMIT);

    recvq.bufs = calloc(recvq.num, sizeof (struct io_buffer));
    assert(recvq.bufs);
    xmitq.bufs = calloc(xmitq.num, sizeof (struct io_buffer));
    assert(xmitq.bufs);

    virtio_net_pci_base = base;
    net_configured = 1;
    intr_register_irq(irq, handle_virtio_net_interrupt, NULL);
    recv_setup();

    /*
     * 8. Set the DRIVER_OK status bit. At this point the device is "live".
     */

    outb(base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_DRIVER_OK);
}

int virtio_net_pkt_poll(void)
{
    if (!net_configured)
        return 0;

    if (recvq.next_avail == recvq.last_used)
        return 0;
    else
        return 1;
}

/* Get the data from the next_avail (top-most) receive buffer/descriptpr in
 * the available ring. */
uint8_t *virtio_net_recv_pkt_get(int *size)
{
    struct io_buffer *buf;

    /* last_used advances whenever we receive a packet, and if it's ahead of
     * next_avail it means that we have a pending packet. */
    if (recvq.next_avail == recvq.last_used)
        return NULL;

    buf = &recvq.bufs[recvq.next_avail];

    /* Remove the virtio_net_hdr */
    *size = buf->len - sizeof(struct virtio_net_hdr);
    return buf->data + sizeof(struct virtio_net_hdr);
}

/* Return the next_avail (top-most) receive buffer/descriptor to the available
 * ring. */
void virtio_net_recv_pkt_put(void)
{
    recvq.bufs[recvq.next_avail].len = PKT_BUFFER_LEN;
    recvq.bufs[recvq.next_avail].extra_flags = VIRTQ_DESC_F_WRITE;
    /* This sets the returned descriptor to be ready for incoming packets, and
     * advances the next_avail index. */
    assert(virtq_add_descriptor_chain(&recvq, recvq.next_avail, 1) == 0);
}

int solo5_net_write_sync(uint8_t *data, int n)
{
    assert(net_configured);

    return virtio_net_xmit_packet(data, n);
}

int solo5_net_read_sync(uint8_t *data, int *n)
{
    uint8_t *pkt;
    int len = *n;

    assert(net_configured);

    pkt = virtio_net_recv_pkt_get(&len);
    if (!pkt)
        return -1;

    assert(len <= *n);
    assert(len <= PKT_BUFFER_LEN);
    *n = len;

    /* also, it's clearly not zero copy */
    memcpy(data, pkt, len);

    virtio_net_recv_pkt_put();

    return 0;
}

char *solo5_net_mac_str(void)
{
    assert(net_configured);

    return virtio_net_mac_str;
}
